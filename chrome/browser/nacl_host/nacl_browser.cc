// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nacl_host/nacl_browser.h"

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
    : irt_platform_file_(base::kInvalidPlatformFileValue),
      irt_filepath_() {
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
      DLOG(ERROR) << "Failed to locate the plugins directory";
      return;
    }

    irt_filepath_ = plugin_dir.Append(NaClIrtName());
  }
}

NaClBrowser* NaClBrowser::GetInstance() {
  return Singleton<NaClBrowser>::get();
}

bool NaClBrowser::IrtAvailable() const {
  return irt_platform_file_ != base::kInvalidPlatformFileValue;
}

base::PlatformFile NaClBrowser::IrtFile() const {
  CHECK_NE(irt_platform_file_, base::kInvalidPlatformFileValue);
  return irt_platform_file_;
}

// Attempt to ensure the IRT will be available when we need it, but don't wait.
bool NaClBrowser::EnsureIrtAvailable() {
  if (IrtAvailable())
    return true;

  return content::BrowserThread::PostTask(
      content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&NaClBrowser::DoOpenIrtLibraryFile));
}

// We really need the IRT to be available now, so make sure that it is.
// When it's ready, we'll run the reply closure.
bool NaClBrowser::MakeIrtAvailable(const base::Closure& reply) {
  return content::BrowserThread::PostTaskAndReply(
      content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&NaClBrowser::DoOpenIrtLibraryFile), reply);
}

const FilePath& NaClBrowser::GetIrtFilePath() {
  return irt_filepath_;
}

// This only ever runs on the BrowserThread::FILE thread.
// If multiple tasks are posted, the later ones are no-ops.
void NaClBrowser::OpenIrtLibraryFile() {
  if (irt_platform_file_ != base::kInvalidPlatformFileValue)
    // We've already run.
    return;

  base::PlatformFileError error_code;
  irt_platform_file_ = base::CreatePlatformFile(irt_filepath_,
                                                base::PLATFORM_FILE_OPEN |
                                                base::PLATFORM_FILE_READ,
                                                NULL,
                                                &error_code);
  if (error_code != base::PLATFORM_FILE_OK) {
    LOG(ERROR) << "Failed to open NaCl IRT file \""
               << irt_filepath_.LossyDisplayName()
               << "\": " << error_code;
  }
}
