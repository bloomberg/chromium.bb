// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/conflicts/module_event_sink_impl_win.h"

#include <windows.h>
#include <psapi.h>

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/conflicts/module_database_win.h"
#include "chrome/common/conflicts/module_watcher_win.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace {

// Gets the path of the module in the provided remote process. Returns true on
// success, false otherwise.
bool GetModulePath(base::ProcessHandle process,
                   HMODULE module,
                   base::FilePath* path) {
  std::vector<wchar_t> temp_path(MAX_PATH);
  size_t length = 0;
  while (true) {
    length = ::GetModuleFileNameEx(process, module, temp_path.data(),
                                   temp_path.size());
    if (length == 0)
      return false;
    if (length < temp_path.size())
      break;
    // The entire buffer was consumed, so grow it to ensure the result wasn't
    // actually truncated.
    temp_path.resize(2 * temp_path.size());
  }

  *path = base::FilePath(base::StringPiece16(temp_path.data(), length));
  return true;
}

// Gets the size of a module in a remote process. Returns true on success, false
// otherwise.
bool GetModuleSize(base::ProcessHandle process,
                   HMODULE module,
                   uint32_t* size) {
  MODULEINFO info = {};
  if (!::GetModuleInformation(process, module, &info, sizeof(info)))
    return false;
  *size = info.SizeOfImage;
  return true;
}

// Reads the typed data from a remote process. Returns true on success, false
// otherwise.
template <typename T>
bool ReadRemoteData(base::ProcessHandle process, uint64_t address, T* data) {
  const void* typed_address =
      reinterpret_cast<const void*>(static_cast<uintptr_t>(address));
  SIZE_T bytes_read = 0;
  if (!::ReadProcessMemory(process, typed_address, data, sizeof(*data),
                           &bytes_read)) {
    return false;
  }
  if (bytes_read != sizeof(*data))
    return false;
  return true;
}

// Reads the time date stamp from the module loaded in the provided remote
// |process| at the provided remote |load_address|.
bool GetModuleTimeDateStamp(base::ProcessHandle process,
                            uint64_t load_address,
                            uint32_t* time_date_stamp) {
  uint64_t address = load_address + offsetof(IMAGE_DOS_HEADER, e_lfanew);
  LONG e_lfanew = 0;
  if (!ReadRemoteData(process, address, &e_lfanew))
    return false;

  address = load_address + e_lfanew + offsetof(IMAGE_NT_HEADERS, FileHeader) +
            offsetof(IMAGE_FILE_HEADER, TimeDateStamp);
  DWORD temp = 0;
  if (!ReadRemoteData(process, address, &temp))
    return false;

  *time_date_stamp = temp;
  return true;
}

}  // namespace

ModuleEventSinkImpl::ModuleEventSinkImpl(base::ProcessHandle process,
                                         content::ProcessType process_type,
                                         ModuleDatabase* module_database)
    : process_(process),
      module_database_(module_database),
      process_id_(0),
      creation_time_(0),
      in_error_(false) {
  // Failing to get basic process information means this channel should not
  // continue to forward data, thus it is marked as being in error.
  process_id_ = ::GetProcessId(process_);
  if (!GetProcessCreationTime(process_, &creation_time_)) {
    in_error_ = true;
    return;
  }
  module_database->OnProcessStarted(process_id_, creation_time_, process_type);
}

ModuleEventSinkImpl::~ModuleEventSinkImpl() = default;

// static
void ModuleEventSinkImpl::Create(GetProcessHandleCallback get_process_handle,
                                 content::ProcessType process_type,
                                 ModuleDatabase* module_database,
                                 mojom::ModuleEventSinkRequest request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::ProcessHandle process = get_process_handle.Run();
  auto module_event_sink_impl = base::MakeUnique<ModuleEventSinkImpl>(
      process, process_type, module_database);
  base::Closure error_handler = base::Bind(
      &ModuleDatabase::OnProcessEnded, base::Unretained(module_database),
      module_event_sink_impl->process_id_,
      module_event_sink_impl->creation_time_);
  auto binding = mojo::MakeStrongBinding(std::move(module_event_sink_impl),
                                         std::move(request));
  binding->set_connection_error_handler(error_handler);
}

void ModuleEventSinkImpl::OnModuleEvent(mojom::ModuleEventType event_type,
                                        uint64_t load_address) {
  if (in_error_)
    return;

  // Mojo takes care of validating |event_type|, so only |load_address| needs to
  // be checked. Load addresses must be aligned with the allocation granularity
  // which is at least 64KB on any supported Windows OS.
  if (load_address == 0 || load_address % (64 * 1024) != 0)
    return;

  switch (event_type) {
    case mojom::ModuleEventType::MODULE_ALREADY_LOADED:
    case mojom::ModuleEventType::MODULE_LOADED:
      return OnModuleLoad(load_address);

    case mojom::ModuleEventType::MODULE_UNLOADED:
      return OnModuleUnload(load_address);
  }
}

// static
bool ModuleEventSinkImpl::GetProcessCreationTime(base::ProcessHandle process,
                                                 uint64_t* creation_time) {
  FILETIME creation_ft = {};
  FILETIME exit_ft = {};
  FILETIME kernel_ft = {};
  FILETIME user_ft = {};
  if (!::GetProcessTimes(process, &creation_ft, &exit_ft, &kernel_ft,
                         &user_ft)) {
    return false;
  }
  *creation_time = (static_cast<uint64_t>(creation_ft.dwHighDateTime) << 32) |
                   static_cast<uint64_t>(creation_ft.dwLowDateTime);
  return true;
}

void ModuleEventSinkImpl::OnModuleLoad(uint64_t load_address) {
  if (in_error_)
    return;

  // The |load_address| is a unique key to a module in a remote process. If
  // there is a valid module there then the following queries should all pass.
  // If any of them fail then the load event is silently swallowed. The entire
  // channel is not marked as being in an error mode, as later events may be
  // well formed.

  // Convert the |load_address| to a module handle.
  HMODULE module =
      reinterpret_cast<HMODULE>(static_cast<uintptr_t>(load_address));

  // Look up the various pieces of module metadata in the remote process.

  base::FilePath module_path;
  if (!GetModulePath(process_, module, &module_path))
    return;

  uint32_t module_size = 0;
  if (!GetModuleSize(process_, module, &module_size))
    return;

  uint32_t module_time_date_stamp = 0;
  if (!GetModuleTimeDateStamp(process_, load_address, &module_time_date_stamp))
    return;

  // Forward this to the module database.
  module_database_->OnModuleLoad(process_id_, creation_time_, module_path,
                                 module_size, module_time_date_stamp,
                                 load_address);
}

void ModuleEventSinkImpl::OnModuleUnload(uint64_t load_address) {
  // Forward this directly to the module database.
  module_database_->OnModuleUnload(process_id_, creation_time_,
                                   static_cast<uintptr_t>(load_address));
}
