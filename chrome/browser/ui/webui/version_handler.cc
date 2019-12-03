// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/version_handler.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/variations/active_field_trials.h"
#include "components/version_ui/version_handler_helper.h"
#include "components/version_ui/version_ui_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/common/content_constants.h"
#include "ppapi/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/plugins/plugin_prefs.h"
#include "content/public/browser/plugin_service.h"
#endif

namespace {

// Retrieves the executable and profile paths on the FILE thread.
void GetFilePaths(const base::FilePath& profile_path,
                  base::string16* exec_path_out,
                  base::string16* profile_path_out) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  base::FilePath executable_path = base::MakeAbsoluteFilePath(
      base::CommandLine::ForCurrentProcess()->GetProgram());
  if (!executable_path.empty())
    *exec_path_out = executable_path.LossyDisplayName();
  else
    *exec_path_out = l10n_util::GetStringUTF16(IDS_VERSION_UI_PATH_NOTFOUND);

  base::FilePath profile_path_copy(base::MakeAbsoluteFilePath(profile_path));
  if (!profile_path.empty() && !profile_path_copy.empty())
    *profile_path_out = profile_path.LossyDisplayName();
  else
    *profile_path_out = l10n_util::GetStringUTF16(IDS_VERSION_UI_PATH_NOTFOUND);
}

}  // namespace

VersionHandler::VersionHandler() {}

VersionHandler::~VersionHandler() {
}

void VersionHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      version_ui::kRequestVersionInfo,
      base::BindRepeating(&VersionHandler::HandleRequestVersionInfo,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      version_ui::kRequestVariationInfo,
      base::BindRepeating(&VersionHandler::HandleRequestVariationInfo,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      version_ui::kRequestPluginInfo,
      base::BindRepeating(&VersionHandler::HandleRequestPluginInfo,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      version_ui::kRequestPathInfo,
      base::BindRepeating(&VersionHandler::HandleRequestPathInfo,
                          base::Unretained(this)));
}

void VersionHandler::HandleRequestVersionInfo(const base::ListValue* args) {
  // This method is overridden by platform-specific handlers which may still
  // use |CallJavascriptFunction|. Main version info is returned by promise
  // using handlers below.
  // TODO(orinj): To fully eliminate chrome.send usage in JS, derived classes
  // could be made to work more like this base class, using
  // |ResolveJavascriptCallback| instead of |CallJavascriptFunction|.
  AllowJavascript();
}

void VersionHandler::HandleRequestVariationInfo(const base::ListValue* args) {
  AllowJavascript();

  std::string callback_id;
  bool include_variations_cmd;
  CHECK_EQ(2U, args->GetSize());
  CHECK(args->GetString(0, &callback_id));
  CHECK(args->GetBoolean(1, &include_variations_cmd));

  base::Value response(base::Value::Type::DICTIONARY);
  response.SetKey(version_ui::kKeyVariationsList,
                  std::move(*version_ui::GetVariationsList()));
  if (include_variations_cmd) {
    response.SetKey(version_ui::kKeyVariationsCmd,
                    version_ui::GetVariationsCommandLineAsValue());
  }
  ResolveJavascriptCallback(base::Value(callback_id), response);
}

void VersionHandler::HandleRequestPluginInfo(const base::ListValue* args) {
  AllowJavascript();

  std::string callback_id;
  CHECK_EQ(1U, args->GetSize());
  CHECK(args->GetString(0, &callback_id));

#if BUILDFLAG(ENABLE_PLUGINS)
  // The Flash version information is needed in the response, so make sure
  // the plugins are loaded.
  content::PluginService::GetInstance()->GetPlugins(
      base::Bind(&VersionHandler::OnGotPlugins, weak_ptr_factory_.GetWeakPtr(),
                 callback_id));
#else
  RejectJavascriptCallback(base::Value(callback_id), base::Value());
#endif
}

void VersionHandler::HandleRequestPathInfo(const base::ListValue* args) {
  AllowJavascript();

  std::string callback_id;
  CHECK_EQ(1U, args->GetSize());
  CHECK(args->GetString(0, &callback_id));

  // Grab the executable path on the FILE thread. It is returned in
  // OnGotFilePaths.
  base::string16* exec_path_buffer = new base::string16;
  base::string16* profile_path_buffer = new base::string16;
  base::PostTaskAndReply(
      FROM_HERE,
      {base::ThreadPool(), base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&GetFilePaths, Profile::FromWebUI(web_ui())->GetPath(),
                     base::Unretained(exec_path_buffer),
                     base::Unretained(profile_path_buffer)),
      base::BindOnce(&VersionHandler::OnGotFilePaths,
                     weak_ptr_factory_.GetWeakPtr(), callback_id,
                     base::Owned(exec_path_buffer),
                     base::Owned(profile_path_buffer)));
}

void VersionHandler::OnGotFilePaths(std::string callback_id,
                                    base::string16* executable_path_data,
                                    base::string16* profile_path_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::Value response(base::Value::Type::DICTIONARY);
  response.SetKey(version_ui::kKeyExecPath, base::Value(*executable_path_data));
  response.SetKey(version_ui::kKeyProfilePath, base::Value(*profile_path_data));
  ResolveJavascriptCallback(base::Value(callback_id), response);
}

#if BUILDFLAG(ENABLE_PLUGINS)
void VersionHandler::OnGotPlugins(
    std::string callback_id,
    const std::vector<content::WebPluginInfo>& plugins) {
  // Obtain the version of the first enabled Flash plugin.
  std::vector<content::WebPluginInfo> info_array;
  content::PluginService::GetInstance()->GetPluginInfoArray(
      GURL(), content::kFlashPluginSwfMimeType, false, &info_array, NULL);
  std::string flash_version_and_path =
      l10n_util::GetStringUTF8(IDS_PLUGINS_DISABLED_PLUGIN);
  PluginPrefs* plugin_prefs =
      PluginPrefs::GetForProfile(Profile::FromWebUI(web_ui())).get();
  if (plugin_prefs) {
    for (size_t i = 0; i < info_array.size(); ++i) {
      if (plugin_prefs->IsPluginEnabled(info_array[i])) {
        flash_version_and_path = base::StringPrintf(
            "%s %s", base::UTF16ToUTF8(info_array[i].version).c_str(),
            base::UTF16ToUTF8(info_array[i].path.LossyDisplayName()).c_str());
        break;
      }
    }
  }
  ResolveJavascriptCallback(base::Value(callback_id),
                            base::Value(flash_version_and_path));
}
#endif  // BUILDFLAG(ENABLE_PLUGINS)
