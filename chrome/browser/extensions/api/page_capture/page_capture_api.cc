// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/page_capture/page_capture_api.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/common/extensions/extension_messages.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

#include <limits>

using content::BrowserThread;
using content::ChildProcessSecurityPolicy;
using content::WebContents;
using extensions::PageCaptureSaveAsMHTMLFunction;
using webkit_blob::ShareableFileReference;

namespace SaveAsMHTML = extensions::api::page_capture::SaveAsMHTML;

namespace {

// Error messages.
const char* const kFileTooBigError = "The MHTML file generated is too big.";
const char* const kMHTMLGenerationFailedError = "Failed to generate MHTML.";
const char* const kTemporaryFileError = "Failed to create a temporary file.";
const char* const kTabClosedError = "Cannot find the tab for thie request.";

}  // namespace

static PageCaptureSaveAsMHTMLFunction::TestDelegate* test_delegate_ = NULL;

PageCaptureSaveAsMHTMLFunction::PageCaptureSaveAsMHTMLFunction() {
}

PageCaptureSaveAsMHTMLFunction::~PageCaptureSaveAsMHTMLFunction() {
  if (mhtml_file_.get()) {
    webkit_blob::ShareableFileReference* to_release = mhtml_file_.get();
    to_release->AddRef();
    mhtml_file_ = NULL;
    BrowserThread::ReleaseSoon(BrowserThread::IO, FROM_HERE, to_release);
  }
}

void PageCaptureSaveAsMHTMLFunction::SetTestDelegate(TestDelegate* delegate) {
  test_delegate_ = delegate;
}

bool PageCaptureSaveAsMHTMLFunction::RunImpl() {
  params_ = SaveAsMHTML::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  AddRef();  // Balanced in ReturnFailure/ReturnSuccess()

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&PageCaptureSaveAsMHTMLFunction::CreateTemporaryFile, this));
  return true;
}

bool PageCaptureSaveAsMHTMLFunction::OnMessageReceivedFromRenderView(
    const IPC::Message& message) {
  if (message.type() != ExtensionHostMsg_ResponseAck::ID)
    return false;

  int message_request_id;
  PickleIterator iter(message);
  if (!message.ReadInt(&iter, &message_request_id)) {
    NOTREACHED() << "malformed extension message";
    return true;
  }

  if (message_request_id != request_id())
    return false;

  // The extension process has processed the response and has created a
  // reference to the blob, it is safe for us to go away.
  Release();  // Balanced in Run()

  return true;
}

void PageCaptureSaveAsMHTMLFunction::CreateTemporaryFile() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  bool success = file_util::CreateTemporaryFile(&mhtml_path_);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&PageCaptureSaveAsMHTMLFunction::TemporaryFileCreated, this,
                 success));
}

void PageCaptureSaveAsMHTMLFunction::TemporaryFileCreated(bool success) {
  if (BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    if (success) {
      // Setup a ShareableFileReference so the temporary file gets deleted
      // once it is no longer used.
      mhtml_file_ = ShareableFileReference::GetOrCreate(
          mhtml_path_, ShareableFileReference::DELETE_ON_FINAL_RELEASE,
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE));
    }
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&PageCaptureSaveAsMHTMLFunction::TemporaryFileCreated, this,
                   success));
    return;
  }

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!success) {
    ReturnFailure(kTemporaryFileError);
    return;
  }

  if (test_delegate_)
    test_delegate_->OnTemporaryFileCreated(mhtml_path_);

  WebContents* web_contents = GetWebContents();
  if (!web_contents) {
    ReturnFailure(kTabClosedError);
    return;
  }

  web_contents->GenerateMHTML(
      mhtml_path_,
      base::Bind(&PageCaptureSaveAsMHTMLFunction::MHTMLGenerated, this));
}

void PageCaptureSaveAsMHTMLFunction::MHTMLGenerated(
    const base::FilePath& file_path,
    int64 mhtml_file_size) {
  DCHECK(mhtml_path_ == file_path);
  if (mhtml_file_size <= 0) {
    ReturnFailure(kMHTMLGenerationFailedError);
    return;
  }

  if (mhtml_file_size > std::numeric_limits<int>::max()) {
    ReturnFailure(kFileTooBigError);
    return;
  }

  ReturnSuccess(mhtml_file_size);
}

void PageCaptureSaveAsMHTMLFunction::ReturnFailure(const std::string& error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  error_ = error;

  SendResponse(false);

  Release();  // Balanced in Run()
}

void PageCaptureSaveAsMHTMLFunction::ReturnSuccess(int64 file_size) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  WebContents* web_contents = GetWebContents();
  if (!web_contents || !render_view_host()) {
    ReturnFailure(kTabClosedError);
    return;
  }

  int child_id = render_view_host()->GetProcess()->GetID();
  ChildProcessSecurityPolicy::GetInstance()->GrantReadFile(
      child_id, mhtml_path_);

  DictionaryValue* dict = new DictionaryValue();
  SetResult(dict);
  dict->SetString("mhtmlFilePath", mhtml_path_.value());
  dict->SetInteger("mhtmlFileLength", file_size);

  SendResponse(true);

  // Note that we'll wait for a response ack message received in
  // OnMessageReceivedFromRenderView before we call Release() (to prevent the
  // blob file from being deleted).
}

WebContents* PageCaptureSaveAsMHTMLFunction::GetWebContents() {
  Browser* browser = NULL;
  content::WebContents* web_contents = NULL;

  if (!ExtensionTabUtil::GetTabById(params_->details.tab_id, profile(),
                                    include_incognito(), &browser, NULL,
                                    &web_contents, NULL)) {
    return NULL;
  }
  return web_contents;
}
