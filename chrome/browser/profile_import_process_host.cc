// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_import_process_host.h"

#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/importer/firefox_importer_utils.h"
#include "chrome/browser/importer/importer_messages.h"
#include "chrome/common/chrome_switches.h"
#include "grit/generated_resources.h"
#include "ipc/ipc_switches.h"
#include "ui/base/l10n/l10n_util.h"

ProfileImportProcessHost::ProfileImportProcessHost(
    ResourceDispatcherHost* resource_dispatcher,
    ImportProcessClient* import_process_client,
    BrowserThread::ID thread_id)
    : BrowserChildProcessHost(PROFILE_IMPORT_PROCESS, resource_dispatcher),
      import_process_client_(import_process_client),
      thread_id_(thread_id) {
}

ProfileImportProcessHost::~ProfileImportProcessHost() {
}

bool ProfileImportProcessHost::StartProfileImportProcess(
    const importer::ProfileInfo& profile_info, int items,
    bool import_to_bookmark_bar) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!StartProcess())
    return false;

  // Dictionary of all localized strings that could be needed by the importer
  // in the external process.
  DictionaryValue localized_strings;
  localized_strings.SetString(
      base::IntToString(IDS_BOOKMARK_GROUP_FROM_FIREFOX),
      l10n_util::GetStringUTF8(IDS_BOOKMARK_GROUP_FROM_FIREFOX));
  localized_strings.SetString(
      base::IntToString(IDS_BOOKMARK_GROUP_FROM_SAFARI),
      l10n_util::GetStringUTF8(IDS_BOOKMARK_GROUP_FROM_SAFARI));
  localized_strings.SetString(
      base::IntToString(IDS_IMPORT_FROM_FIREFOX),
      l10n_util::GetStringUTF8(IDS_IMPORT_FROM_FIREFOX));
  localized_strings.SetString(
      base::IntToString(IDS_IMPORT_FROM_GOOGLE_TOOLBAR),
      l10n_util::GetStringUTF8(IDS_IMPORT_FROM_GOOGLE_TOOLBAR));
  localized_strings.SetString(
      base::IntToString(IDS_IMPORT_FROM_SAFARI),
      l10n_util::GetStringUTF8(IDS_IMPORT_FROM_SAFARI));

  Send(new ProfileImportProcessMsg_StartImport(
      profile_info, items, localized_strings, import_to_bookmark_bar));
  return true;
}

bool ProfileImportProcessHost::CancelProfileImportProcess() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  Send(new ProfileImportProcessMsg_CancelImport());
  return true;
}

bool ProfileImportProcessHost::ReportImportItemFinished(
    importer::ImportItem item) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  Send(new ProfileImportProcessMsg_ReportImportItemFinished(item));
  return true;
}

FilePath ProfileImportProcessHost::GetProfileImportProcessCmd() {
  return GetChildPath(true);
}

bool ProfileImportProcessHost::StartProcess() {
  set_name(L"profile import process");

  if (!CreateChannel())
    return false;

  FilePath exe_path = GetProfileImportProcessCmd();
  if (exe_path.empty()) {
    NOTREACHED() << "Unable to get profile import process binary name.";
    return false;
  }

  CommandLine* cmd_line = new CommandLine(exe_path);
  cmd_line->AppendSwitchASCII(switches::kProcessType,
                              switches::kProfileImportProcess);
  cmd_line->AppendSwitchASCII(switches::kProcessChannelID, channel_id());

  SetCrashReporterCommandLine(cmd_line);

  const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();
  if (browser_command_line.HasSwitch(switches::kChromeFrame))
    cmd_line->AppendSwitch(switches::kChromeFrame);

#if defined(OS_MACOSX)
  base::environment_vector env;
  std::string dylib_path = GetFirefoxDylibPath().value();
  if (!dylib_path.empty())
    env.push_back(std::make_pair("DYLD_FALLBACK_LIBRARY_PATH", dylib_path));

  Launch(false, env, cmd_line);
#elif defined(OS_WIN)
  FilePath no_exposed_directory;

  Launch(no_exposed_directory, cmd_line);
#else
  base::environment_vector env;

  Launch(false, env, cmd_line);
#endif

  return true;
}

bool ProfileImportProcessHost::OnMessageReceived(const IPC::Message& message) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(
      thread_id_, FROM_HERE,
      NewRunnableMethod(import_process_client_.get(),
                        &ImportProcessClient::OnMessageReceived,
                        message));
  return true;
}

void ProfileImportProcessHost::OnProcessCrashed(int exit_code) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(
      thread_id_, FROM_HERE,
      NewRunnableMethod(import_process_client_.get(),
                        &ImportProcessClient::OnProcessCrashed,
                        exit_code));
}

bool ProfileImportProcessHost::CanShutdown() {
  return true;
}

ProfileImportProcessHost::ImportProcessClient::ImportProcessClient() {}

ProfileImportProcessHost::ImportProcessClient::~ImportProcessClient() {}

bool ProfileImportProcessHost::ImportProcessClient::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ProfileImportProcessHost, message)
    // Notification messages about the state of the import process.
    IPC_MESSAGE_HANDLER(ProfileImportProcessHostMsg_Import_Started,
                        ImportProcessClient::OnImportStart)
    IPC_MESSAGE_HANDLER(ProfileImportProcessHostMsg_Import_Finished,
                        ImportProcessClient::OnImportFinished)
    IPC_MESSAGE_HANDLER(ProfileImportProcessHostMsg_ImportItem_Started,
                        ImportProcessClient::OnImportItemStart)
    IPC_MESSAGE_HANDLER(ProfileImportProcessHostMsg_ImportItem_Finished,
                        ImportProcessClient::OnImportItemFinished)

    // Data messages containing items to be written to the user profile.
    IPC_MESSAGE_HANDLER(ProfileImportProcessHostMsg_NotifyHistoryImportStart,
                        ImportProcessClient::OnHistoryImportStart)
    IPC_MESSAGE_HANDLER(ProfileImportProcessHostMsg_NotifyHistoryImportGroup,
                        ImportProcessClient::OnHistoryImportGroup)
    IPC_MESSAGE_HANDLER(ProfileImportProcessHostMsg_NotifyHomePageImportReady,
                        ImportProcessClient::OnHomePageImportReady)
    IPC_MESSAGE_HANDLER(ProfileImportProcessHostMsg_NotifyBookmarksImportStart,
                        ImportProcessClient::OnBookmarksImportStart)
    IPC_MESSAGE_HANDLER(ProfileImportProcessHostMsg_NotifyBookmarksImportGroup,
                        ImportProcessClient::OnBookmarksImportGroup)
    IPC_MESSAGE_HANDLER(ProfileImportProcessHostMsg_NotifyFaviconsImportStart,
                        ImportProcessClient::OnFaviconsImportStart)
    IPC_MESSAGE_HANDLER(ProfileImportProcessHostMsg_NotifyFaviconsImportGroup,
                        ImportProcessClient::OnFaviconsImportGroup)
    IPC_MESSAGE_HANDLER(ProfileImportProcessHostMsg_NotifyPasswordFormReady,
                        ImportProcessClient::OnPasswordFormImportReady)
    IPC_MESSAGE_HANDLER(ProfileImportProcessHostMsg_NotifyKeywordsReady,
                        ImportProcessClient::OnKeywordsImportReady)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}
