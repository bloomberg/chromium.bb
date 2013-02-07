// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string16.h"
#include "content/browser/hyphenator/hyphenator_message_filter.h"
#include "content/common/hyphenator_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"

namespace content {

namespace {

// A helper function that closes the specified file in the FILE thread. This
// function may be called after the HyphenatorMessageFilter object that owns the
// specified file is deleted, i.e. this function must not depend on the object.
void CloseDictionary(base::PlatformFile file) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  base::ClosePlatformFile(file);
}

}  // namespace

HyphenatorMessageFilter::HyphenatorMessageFilter(
    RenderProcessHost* render_process_host)
    : render_process_host_(render_process_host),
      dictionary_file_(base::kInvalidPlatformFileValue),
      weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

HyphenatorMessageFilter::~HyphenatorMessageFilter() {
  // Post a FILE task that deletes the dictionary file. This message filter is
  // usually deleted on the IO thread, which does not allow file operations.
  if (dictionary_file_ != base::kInvalidPlatformFileValue) {
    BrowserThread::PostTask(
        BrowserThread::FILE,
        FROM_HERE,
        base::Bind(&CloseDictionary, dictionary_file_));
  }
}

void HyphenatorMessageFilter::SetDictionaryBase(const base::FilePath& base) {
  dictionary_base_ = base;
}

void HyphenatorMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message,
    BrowserThread::ID* thread) {
  if (message.type() == HyphenatorHostMsg_OpenDictionary::ID)
    *thread = BrowserThread::UI;
}

bool HyphenatorMessageFilter::OnMessageReceived(
    const IPC::Message& message,
    bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(HyphenatorMessageFilter,
                           message,
                           *message_was_ok)
    IPC_MESSAGE_HANDLER(HyphenatorHostMsg_OpenDictionary, OnOpenDictionary)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

void HyphenatorMessageFilter::OnOpenDictionary(const string16& locale) {
  if (dictionary_file_ != base::kInvalidPlatformFileValue) {
    SendDictionary();
    return;
  }
  BrowserThread::PostTaskAndReply(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&HyphenatorMessageFilter::OpenDictionary, this, locale),
      base::Bind(&HyphenatorMessageFilter::SendDictionary,
                 weak_factory_.GetWeakPtr()));
}

void HyphenatorMessageFilter::OpenDictionary(const string16& locale) {
  DCHECK(dictionary_file_ == base::kInvalidPlatformFileValue);

  if (dictionary_base_.empty()) {
    dictionary_base_ =
        GetContentClient()->browser()->GetHyphenDictionaryDirectory();
  }
  std::string rule_file = locale.empty() ? "en-US" : UTF16ToASCII(locale);

  // Currently, only en-US is hyphenated. This is a quick fix for
  // http://crbug.com/167122.
  // TODO(groby): The proper fix entails validating if locale is a properly
  // formatted locale string, but knowledge about valid locales currently
  // resides in chrome, not content.
  if (rule_file != "en-US")
    return;
  rule_file.append("-1-0.dic");
  base::FilePath rule_path = dictionary_base_.AppendASCII(rule_file);
  dictionary_file_ = base::CreatePlatformFile(
      rule_path,
      base::PLATFORM_FILE_READ | base::PLATFORM_FILE_OPEN,
      NULL, NULL);
}

void HyphenatorMessageFilter::SendDictionary() {
  IPC::PlatformFileForTransit file = IPC::InvalidPlatformFileForTransit();
  if (dictionary_file_ != base::kInvalidPlatformFileValue) {
    file = IPC::GetFileHandleForProcess(
        dictionary_file_,
        render_process_host_->GetHandle(),
        false);
  }
  Send(new HyphenatorMsg_SetDictionary(file));
}

}  // namespace content
