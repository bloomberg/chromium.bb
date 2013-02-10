// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/version_handler.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/metrics/field_trial.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/metrics/variations/variations_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/web_ui.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "webkit/plugins/plugin_constants.h"

namespace {

// Retrieves the executable and profile paths on the FILE thread.
void GetFilePaths(const base::FilePath& profile_path,
                  string16* exec_path_out,
                  string16* profile_path_out) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));

  base::FilePath executable_path =
      CommandLine::ForCurrentProcess()->GetProgram();
  if (file_util::AbsolutePath(&executable_path)) {
    *exec_path_out = executable_path.LossyDisplayName();
  } else {
    *exec_path_out =
        l10n_util::GetStringUTF16(IDS_ABOUT_VERSION_PATH_NOTFOUND);
  }

  base::FilePath profile_path_copy(profile_path);
  if (!profile_path.empty() && file_util::AbsolutePath(&profile_path_copy)) {
    *profile_path_out = profile_path.LossyDisplayName();
  } else {
    *profile_path_out =
        l10n_util::GetStringUTF16(IDS_ABOUT_VERSION_PATH_NOTFOUND);
  }
}

}  // namespace

VersionHandler::VersionHandler()
    : weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

VersionHandler::~VersionHandler() {
}

void VersionHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "requestVersionInfo",
      base::Bind(&VersionHandler::HandleRequestVersionInfo,
      base::Unretained(this)));
}

void VersionHandler::HandleRequestVersionInfo(const ListValue* args) {
#if defined(ENABLE_PLUGINS)
  // The Flash version information is needed in the response, so make sure
  // the plugins are loaded.
  content::PluginService::GetInstance()->GetPlugins(
      base::Bind(&VersionHandler::OnGotPlugins,
          weak_ptr_factory_.GetWeakPtr()));
#endif

  // Grab the executable path on the FILE thread. It is returned in
  // OnGotFilePaths.
  string16* exec_path_buffer = new string16;
  string16* profile_path_buffer = new string16;
  content::BrowserThread::PostTaskAndReply(
      content::BrowserThread::FILE, FROM_HERE,
          base::Bind(&GetFilePaths, Profile::FromWebUI(web_ui())->GetPath(),
                     base::Unretained(exec_path_buffer),
                     base::Unretained(profile_path_buffer)),
          base::Bind(&VersionHandler::OnGotFilePaths,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::Owned(exec_path_buffer),
                     base::Owned(profile_path_buffer)));

  // Respond with the variations info immediately.
  scoped_ptr<ListValue> variations_list(new ListValue());
  std::vector<std::string> variations;
#if !defined(NDEBUG)
  base::FieldTrial::ActiveGroups active_groups;
  base::FieldTrialList::GetActiveFieldTrialGroups(&active_groups);

  const unsigned char kNonBreakingHyphenUTF8[] = { 0xE2, 0x80, 0x91, '\0' };
  const std::string kNonBreakingHyphenUTF8String(
      reinterpret_cast<const char*>(kNonBreakingHyphenUTF8));
  for (size_t i = 0; i < active_groups.size(); ++i) {
    std::string line = active_groups[i].trial_name + ":" +
                       active_groups[i].group_name;
    ReplaceChars(line, "-", kNonBreakingHyphenUTF8String, &line);
    variations.push_back(line);
  }
#else
  // In release mode, display the hashes only.
  std::vector<string16> active_groups;
  chrome_variations::GetFieldTrialActiveGroupIdsAsStrings(&active_groups);
  for (size_t i = 0; i < active_groups.size(); ++i)
    variations.push_back(UTF16ToASCII(active_groups[i]));
#endif

  for (std::vector<std::string>::const_iterator it = variations.begin();
      it != variations.end(); ++it) {
    variations_list->Append(Value::CreateStringValue(*it));
  }

  // In release mode, this will return an empty list to clear the section.
  web_ui()->CallJavascriptFunction("returnVariationInfo",
                                   *variations_list.release());
}

void VersionHandler::OnGotFilePaths(string16* executable_path_data,
                                    string16* profile_path_data) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  StringValue exec_path(*executable_path_data);
  StringValue profile_path(*profile_path_data);
  web_ui()->CallJavascriptFunction("returnFilePaths", exec_path, profile_path);
}

#if defined(ENABLE_PLUGINS)
void VersionHandler::OnGotPlugins(
    const std::vector<webkit::WebPluginInfo>& plugins) {
  // Obtain the version of the first enabled Flash plugin.
  std::vector<webkit::WebPluginInfo> info_array;
  content::PluginService::GetInstance()->GetPluginInfoArray(
      GURL(), kFlashPluginSwfMimeType, false, &info_array, NULL);
  string16 flash_version =
      l10n_util::GetStringUTF16(IDS_PLUGINS_DISABLED_PLUGIN);
  PluginPrefs* plugin_prefs =
      PluginPrefs::GetForProfile(Profile::FromWebUI(web_ui()));
  if (plugin_prefs) {
    for (size_t i = 0; i < info_array.size(); ++i) {
      if (plugin_prefs->IsPluginEnabled(info_array[i])) {
        flash_version = info_array[i].version;
        break;
      }
    }
  }

  StringValue arg(flash_version);
  web_ui()->CallJavascriptFunction("returnFlashVersion", arg);
}
#endif  // defined(ENABLE_PLUGINS)
