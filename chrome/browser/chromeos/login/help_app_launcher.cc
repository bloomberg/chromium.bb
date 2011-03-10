// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/help_app_launcher.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browser_thread.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

namespace {
const char kHelpTopicBasePath[] = "/usr/share/chromeos-assets/help/";

const char* kHelpTopicFiles[] = { "connectivity.html",
                                  "usage.html",
                                  "accessaccount.html",
                                  "password.html",
                                  "disabled.html",
                                  "hosted.html" };
}  // namespace

COMPILE_ASSERT(arraysize(kHelpTopicFiles) == HelpAppLauncher::HELP_TOPIC_COUNT,
               help_topic_paths_should_match_help_topic_enum_count);

///////////////////////////////////////////////////////////////////////////////
// HelpApp, public:

HelpAppLauncher::HelpAppLauncher(gfx::NativeWindow parent_window)
    : help_topic_id_(HELP_CANT_ACCESS_ACCOUNT),
      parent_window_(parent_window) {
}

void HelpAppLauncher::ShowHelpTopic(HelpTopic help_topic_id) {
  if (help_topic_id < 0 || help_topic_id >= HELP_TOPIC_COUNT) {
    LOG(ERROR) << "Unknown help topic ID was requested: " << help_topic_id;
    return;
  }
  help_topic_id_ = help_topic_id;

  // TODO(nkostylev): Detect connectivity state (offline/online).
  // Help presentation may wary based on that (dialog/launch BWSI mode).
  std::string locale = g_browser_process->GetApplicationLocale();
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
                        &HelpAppLauncher::FindStaticHelpTopic,
                        kHelpTopicFiles[help_topic_id],
                        locale));
}

///////////////////////////////////////////////////////////////////////////////
// HelpApp, private:

void HelpAppLauncher::FindStaticHelpTopic(const std::string& filename,
                                          const std::string& locale) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  FilePath file_path(kHelpTopicBasePath);
  file_path = file_path.Append(locale);
  file_path = file_path.Append(filename);

  if (!file_util::PathExists(file_path)) {
    // Check default location if locale specific help content is absent.
    file_path = FilePath(kHelpTopicBasePath + filename);
    if (!file_util::PathExists(file_path)) {
      LOG(ERROR) << "Help topic file was not found. ID: " << help_topic_id_;
      return;
    }
  }

  const std::string path_url = std::string(chrome::kFileScheme) +
       chrome::kStandardSchemeSeparator + file_path.value();
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this,
                        &HelpAppLauncher::ShowHelpTopicDialog,
                        GURL(path_url)));
}

void HelpAppLauncher::ShowHelpTopicDialog(const GURL& topic_url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!dialog_.get()) {
    dialog_.reset(new LoginHtmlDialog(
        this,
        parent_window_,
        UTF16ToWide(
            l10n_util::GetStringUTF16(IDS_LOGIN_OOBE_HELP_DIALOG_TITLE)),
        topic_url,
        LoginHtmlDialog::STYLE_BUBBLE));
  } else {
    dialog_->set_url(topic_url);
  }
  dialog_->Show();
}

}  // namespace chromeos
