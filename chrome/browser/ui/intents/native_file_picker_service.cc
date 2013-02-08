// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides the UI implementation of the classes declared
// in chrome/browser/intents/native_services.h and partially defined in
// chrome/browser/intents/native_services.cc.

#include <vector>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/platform_file.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/intents/intent_service_host.h"
#include "chrome/browser/intents/native_services.h"
#include "chrome/browser/intents/web_intents_util.h"
#include "chrome/browser/platform_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_intents_dispatcher.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "net/base/mime_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "webkit/glue/web_intent_data.h"
#include "webkit/glue/web_intent_service_data.h"

namespace web_intents {
namespace {

const int kInvalidFileSize = -1;

// Returns a FileTypeInfo instance representative of |mime_type|.
ui::SelectFileDialog::FileTypeInfo TypeInfoFromMimeType(
    const std::string& mime_type) {
  ui::SelectFileDialog::FileTypeInfo info;
  info.include_all_files = true;
  info.extensions.resize(1);
  net::GetExtensionsForMimeType(mime_type, &info.extensions.back());

  // Provide a "helpful" description when possible.
  int description_id = 0;
  if (mime_type == "image/*") {
    description_id = IDS_IMAGE_FILES;
  } else if (mime_type == "audio/*") {
    description_id = IDS_AUDIO_FILES;
  } else if (mime_type == "video/*") {
    description_id = IDS_VIDEO_FILES;
  }

  if (description_id != 0) {
    info.extension_description_overrides.push_back(
        l10n_util::GetStringUTF16(description_id));
  }

  return info;
}

// FilePicker service allowing a native file picker to handle
// pick + */* intents.
class NativeFilePickerService
    : public IntentServiceHost, public ui::SelectFileDialog::Listener {
 public:
  explicit NativeFilePickerService(content::WebContents* web_contents);
  virtual ~NativeFilePickerService();

  // Reads the length of the file on the FILE thread, then returns
  // the file and the length to the UI thread.
  void ReadFileLength(const FilePath& path);

  // Handles sending of data back to dispatcher
  void PostDataFileReply(const FilePath& path, int64 length);

  // Implements IntentServiceHost:
  virtual void HandleIntent(content::WebIntentsDispatcher* dispatcher) OVERRIDE;

  // Implements SelectFileDialog::Listener
  virtual void FileSelected(
      const FilePath& path, int index, void* params) OVERRIDE;
  virtual void FileSelectionCanceled(void* params) OVERRIDE;

 private:
  // The web contents on which the selector will be displayed. Not owned.
  content::WebContents* web_contents_;

  // The dispatcher for the current intent. Only set at the time the intent
  // request is delivered to HandleIntent. Not owned.
  content::WebIntentsDispatcher* dispatcher_;

  scoped_refptr<ui::SelectFileDialog> dialog_;

  DISALLOW_COPY_AND_ASSIGN(NativeFilePickerService);
};

}  // namespace

NativeFilePickerService::NativeFilePickerService(
    content::WebContents* web_contents)
    : web_contents_(web_contents), dispatcher_(NULL) {
}

NativeFilePickerService::~NativeFilePickerService() {}

void NativeFilePickerService::HandleIntent(
    content::WebIntentsDispatcher* dispatcher) {
  DCHECK(dispatcher);
  dispatcher_ = dispatcher;

  dialog_ = ui::SelectFileDialog::Create(this, NULL);

  ui::SelectFileDialog::FileTypeInfo type_info =
      TypeInfoFromMimeType(UTF16ToASCII(dispatcher_->GetIntent().type));

  dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_OPEN_FILE,
      FilePickerFactory::GetServiceTitle(),
      FilePath(FILE_PATH_LITERAL(".")),
      &type_info,
      1,  // Index of which file description to show.
      FILE_PATH_LITERAL(""),
      platform_util::GetTopLevel(web_contents_->GetNativeView()),
      NULL);
}

void NativeFilePickerService::ReadFileLength(const FilePath& path) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));

  int64 file_size;
  if (!file_util::GetFileSize(path, &file_size))
    file_size = kInvalidFileSize;

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&NativeFilePickerService::PostDataFileReply,
                 base::Unretained(this), path, file_size));
}

void NativeFilePickerService::PostDataFileReply(
    const FilePath& path, int64 length) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (length <= kInvalidFileSize) {
    DLOG(WARNING) << "Unable to determine file size.";
    dispatcher_->SendReply(
        webkit_glue::WebIntentReply(
            webkit_glue::WEB_INTENT_REPLY_FAILURE, string16()));
    return;
  }

  content::ChildProcessSecurityPolicy::GetInstance()->GrantReadFile(
      web_contents_->GetRenderProcessHost()->GetID(),
      path);

  dispatcher_->SendReply(webkit_glue::WebIntentReply(
      webkit_glue::WEB_INTENT_REPLY_SUCCESS, path, length));
}

void NativeFilePickerService::FileSelected(
    const FilePath& path, int index, void* params) {
  DCHECK(dispatcher_);
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(
          &NativeFilePickerService::ReadFileLength,
          base::Unretained(this), path));
}

void NativeFilePickerService::FileSelectionCanceled(void* params) {
  DCHECK(dispatcher_);
  dispatcher_->SendReply(
      webkit_glue::WebIntentReply(
          webkit_glue::WEB_INTENT_REPLY_FAILURE, string16()));
}

// static
IntentServiceHost* FilePickerFactory::CreateServiceInstance(
    const webkit_glue::WebIntentData& intent,
    content::WebContents* web_contents) {
  return new NativeFilePickerService(web_contents);
}

string16 FilePickerFactory::GetServiceTitle() {
  return l10n_util::GetStringUTF16(IDS_WEB_INTENTS_FILE_PICKER_SERVICE_TITLE);
}

}  // namespace web_intents
