// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nacl_host/nacl_browser.h"

#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/win/windows_version.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_thread.h"

namespace {

// Determine the name of the IRT file based on the architecture.
#define NACL_IRT_FILE_NAME(arch_string) \
  (FILE_PATH_LITERAL("nacl_irt_")       \
   FILE_PATH_LITERAL(arch_string)       \
   FILE_PATH_LITERAL(".nexe"))

const FilePath::StringType NaClIrtName() {
#if defined(ARCH_CPU_X86_FAMILY)
#if defined(ARCH_CPU_X86_64)
  bool is64 = true;
#elif defined(OS_WIN)
  bool is64 = (base::win::OSInfo::GetInstance()->wow64_status() ==
               base::win::OSInfo::WOW64_ENABLED);
#else
  bool is64 = false;
#endif
  return is64 ? NACL_IRT_FILE_NAME("x86_64") : NACL_IRT_FILE_NAME("x86_32");
#elif defined(ARCH_CPU_ARMEL)
  // TODO(mcgrathr): Eventually we'll need to distinguish arm32 vs thumb2.
  // That may need to be based on the actual nexe rather than a static
  // choice, which would require substantial refactoring.
  return NACL_IRT_FILE_NAME("arm");
#else
#error Add support for your architecture to NaCl IRT file selection
#endif
}

}  // namespace

NaClBrowser::NaClBrowser()
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      irt_platform_file_(base::kInvalidPlatformFileValue),
      irt_filepath_(),
      irt_state_(NaClResourceUninitialized),
      ok_(true) {
  InitIrtFilePath();
}

NaClBrowser::~NaClBrowser() {
  if (irt_platform_file_ != base::kInvalidPlatformFileValue)
    base::ClosePlatformFile(irt_platform_file_);
}

void NaClBrowser::InitIrtFilePath() {
  // Allow the IRT library to be overridden via an environment
  // variable.  This allows the NaCl/Chromium integration bot to
  // specify a newly-built IRT rather than using a prebuilt one
  // downloaded via Chromium's DEPS file.  We use the same environment
  // variable that the standalone NaCl PPAPI plugin accepts.
  const char* irt_path_var = getenv("NACL_IRT_LIBRARY");
  if (irt_path_var != NULL) {
    FilePath::StringType path_string(
        irt_path_var, const_cast<const char*>(strchr(irt_path_var, '\0')));
    irt_filepath_ = FilePath(path_string);
  } else {
    FilePath plugin_dir;
    if (!PathService::Get(chrome::DIR_INTERNAL_PLUGINS, &plugin_dir)) {
      DLOG(ERROR) << "Failed to locate the plugins directory, NaCl disabled.";
      MarkAsFailed();
      return;
    }
    irt_filepath_ = plugin_dir.Append(NaClIrtName());
  }
}

NaClBrowser* NaClBrowser::GetInstance() {
  return Singleton<NaClBrowser>::get();
}

bool NaClBrowser::IsReady() const {
  return IsOk() && irt_state_ == NaClResourceReady;
}

bool NaClBrowser::IsOk() const {
  return ok_;
}

base::PlatformFile NaClBrowser::IrtFile() const {
  CHECK_EQ(irt_state_, NaClResourceReady);
  CHECK_NE(irt_platform_file_, base::kInvalidPlatformFileValue);
  return irt_platform_file_;
}

void NaClBrowser::EnsureAllResourcesAvailable() {
  // Currently the only resource we need to initialize is the IRT.
  // In the future we will need to load the validation cache from disk.
  EnsureIrtAvailable();
}

// Load the IRT async.
void NaClBrowser::EnsureIrtAvailable() {
  if (IsOk() && irt_state_ == NaClResourceUninitialized) {
    irt_state_ = NaClResourceRequested;
    // TODO(ncbray) use blocking pool.
    if (!base::FileUtilProxy::CreateOrOpen(
            content::BrowserThread::GetMessageLoopProxyForThread(
                content::BrowserThread::FILE),
            irt_filepath_,
            base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ,
            base::Bind(&NaClBrowser::OnIrtOpened,
                       weak_factory_.GetWeakPtr()))) {
      LOG(ERROR) << "Internal error, NaCl disabled.";
      MarkAsFailed();
    }
  }
}

void NaClBrowser::OnIrtOpened(base::PlatformFileError error_code,
                              base::PassPlatformFile file,
                              bool created) {
  DCHECK_EQ(irt_state_, NaClResourceRequested);
  DCHECK(!created);
  if (error_code == base::PLATFORM_FILE_OK) {
    irt_platform_file_ = file.ReleaseValue();
  } else {
    LOG(ERROR) << "Failed to open NaCl IRT file \""
               << irt_filepath_.LossyDisplayName()
               << "\": " << error_code;
    MarkAsFailed();
  }
  irt_state_ = NaClResourceReady;
  CheckWaiting();
}

void NaClBrowser::CheckWaiting() {
  if (!IsOk() || IsReady()) {
    // Queue the waiting tasks into the message loop.  This helps avoid
    // re-entrancy problems that could occur if the closure was invoked
    // directly.  For example, this could result in use-after-free of the
    // process host.
    for (std::vector<base::Closure>::iterator iter = waiting_.begin();
         iter != waiting_.end(); ++iter) {
      MessageLoop::current()->PostTask(FROM_HERE, *iter);
    }
    waiting_.clear();
  }
}

void NaClBrowser::MarkAsFailed() {
  ok_ = false;
  CheckWaiting();
}

void NaClBrowser::WaitForResources(const base::Closure& reply) {
  waiting_.push_back(reply);
  EnsureAllResourcesAvailable();
  CheckWaiting();
}

const FilePath& NaClBrowser::GetIrtFilePath() {
  return irt_filepath_;
}
