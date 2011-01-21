// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/help_app_launcher.h"
#include "chrome/common/url_constants.h"
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
    : parent_window_(parent_window) {
}

// Checks whether local file exists at specified base path and
// returns GURL instance for it. Otherwise returns an empty GURL.
static GURL GetLocalFileUrl(const std::string& base_path,
                            const std::string& filename) {
  FilePath file_path(base_path + filename);
  if (file_util::PathExists(file_path)) {
    const std::string path_url = std::string(chrome::kFileScheme) +
         chrome::kStandardSchemeSeparator + file_path.value();
    return GURL(path_url);
  } else {
    return GURL();
  }
}

void HelpAppLauncher::ShowHelpTopic(HelpTopic help_topic_id) {
  if (help_topic_id < 0 || help_topic_id >= HELP_TOPIC_COUNT) {
    LOG(ERROR) << "Unknown help topic ID was requested: " << help_topic_id;
    return;
  }

  // TODO(nkostylev): Detect connectivity state (offline/online).
  // Help presentation may wary based on that (dialog/launch BWSI mode).
  GURL url = GetLocalFileUrl(kHelpTopicBasePath,
                             kHelpTopicFiles[help_topic_id]);
  if (!url.is_empty()) {
    ShowHelpTopicDialog(url);
  } else {
    LOG(ERROR) << "Help topic static file was not found. ID: " << help_topic_id;
  }
}

///////////////////////////////////////////////////////////////////////////////
// HelpApp, private:

void HelpAppLauncher::ShowHelpTopicDialog(const GURL& topic_url) {
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
