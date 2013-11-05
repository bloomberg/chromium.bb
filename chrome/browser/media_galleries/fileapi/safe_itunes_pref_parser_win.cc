// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/safe_itunes_pref_parser_win.h"

#include "chrome/common/chrome_utility_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/utility_process_host.h"

using content::BrowserThread;
using content::UtilityProcessHost;

namespace itunes {

SafeITunesPrefParserWin::SafeITunesPrefParserWin(
    const std::string& unsafe_xml,
    const ParserCallback& callback)
    : unsafe_xml_(unsafe_xml),
      callback_(callback),
      parser_state_(INITIAL_STATE) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(!callback_.is_null());
}

void SafeITunesPrefParserWin::Start() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&SafeITunesPrefParserWin::StartWorkOnIOThread, this));
}

SafeITunesPrefParserWin::~SafeITunesPrefParserWin() {
}

void SafeITunesPrefParserWin::StartWorkOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK_EQ(INITIAL_STATE, parser_state_);

  UtilityProcessHost* host =
      UtilityProcessHost::Create(this, base::MessageLoopProxy::current());
  host->Send(new ChromeUtilityMsg_ParseITunesPrefXml(unsafe_xml_));
  parser_state_ = STARTED_PARSING_STATE;
}

void SafeITunesPrefParserWin::OnGotITunesDirectory(
    const base::FilePath& library_file) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (parser_state_ != STARTED_PARSING_STATE)
    return;
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                          base::Bind(callback_, library_file));
  parser_state_ = FINISHED_PARSING_STATE;
}

void SafeITunesPrefParserWin::OnProcessCrashed(int exit_code) {
  OnGotITunesDirectory(base::FilePath());
}

bool SafeITunesPrefParserWin::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SafeITunesPrefParserWin, message)
    IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_GotITunesDirectory,
                        OnGotITunesDirectory)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

}  // namespace itunes
