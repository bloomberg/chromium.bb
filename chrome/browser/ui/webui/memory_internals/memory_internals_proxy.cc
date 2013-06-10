// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/memory_internals/memory_internals_proxy.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/memory_details.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/memory_internals/memory_internals_handler.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_set.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui.h"
#include "grit/chromium_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/webui/jstemplate_builder.h"
#include "ui/webui/web_ui_util.h"

using content::BrowserThread;

namespace {

class BrowserProcessDetails : public MemoryDetails {
 public:
  typedef base::Callback<void(const ProcessData&)> DataCallback;
  explicit BrowserProcessDetails(const DataCallback& callback)
      : callback_(callback) {}
  virtual void OnDetailsAvailable() OVERRIDE {
    const std::vector<ProcessData>& browser_processes = processes();
    // [0] means Chrome.
    callback_.Run(browser_processes[0]);
  }

 private:
  virtual ~BrowserProcessDetails() {}

  DataCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(BrowserProcessDetails);
};

}  // namespace

MemoryInternalsProxy::MemoryInternalsProxy() {}

void MemoryInternalsProxy::Attach(MemoryInternalsHandler* handler) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  handler_ = handler;
}

void MemoryInternalsProxy::Detach() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  handler_ = NULL;
}

void MemoryInternalsProxy::GetInfo(const base::ListValue* list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  scoped_refptr<BrowserProcessDetails> browsers(new BrowserProcessDetails(
      base::Bind(&MemoryInternalsProxy::OnDetailsAvailable, this)));
  browsers->StartFetch(MemoryDetails::SKIP_USER_METRICS);
}

MemoryInternalsProxy::~MemoryInternalsProxy() {}

void MemoryInternalsProxy::UpdateUIOnUIThread(const string16& update) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Don't forward updates to a destructed UI.
  if (handler_)
    handler_->OnUpdate(update);
}

void MemoryInternalsProxy::OnDetailsAvailable(const ProcessData& browser) {
  base::DictionaryValue details;

  // System information, which is independent from processes.
  details.SetInteger("uptime", base::SysInfo::Uptime());
  details.SetString("os", base::SysInfo::OperatingSystemName());
  details.SetString("os_version", base::SysInfo::OperatingSystemVersion());

  base::ListValue* processes = new ListValue();
  base::ListValue* extensions = new ListValue();
  details.Set("processes", processes);
  details.Set("extensions", extensions);
  for (ProcessMemoryInformationList::const_iterator
           iter = browser.processes.begin();
       iter != browser.processes.end(); ++iter) {
    base::DictionaryValue* info = new DictionaryValue();

    // Information from MemoryDetails.
    info->SetInteger("pid", iter->pid);
    info->SetString("type",
                    ProcessMemoryInformation::GetFullTypeNameInEnglish(
                        iter->process_type, iter->renderer_type));
    info->SetInteger("memory_private",
                     iter->working_set.priv + iter->committed.priv);
    base::ListValue* titles = new ListValue();
    info->Set("titles", titles);
    for (size_t i = 0; i < iter->titles.size(); ++i)
      titles->AppendString(iter->titles[i]);

    if (iter->process_type == content::PROCESS_TYPE_RENDERER &&
        iter->renderer_type == ProcessMemoryInformation::RENDERER_EXTENSION)
      extensions->Append(info);
    else
      processes->Append(info);
  }

  CallJavaScriptFunctionOnUIThread("g_main_view.onSetSnapshot", &details);
}

void MemoryInternalsProxy::CallJavaScriptFunctionOnUIThread(
    const std::string& function, base::Value* args) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  std::vector<const base::Value*> args_vector;
  args_vector.push_back(args);
  string16 update = content::WebUI::GetJavascriptCall(function, args_vector);
  UpdateUIOnUIThread(update);
}
