// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/profiler_ui.h"

#include <string>

// When testing the javacript code, it is cumbersome to have to keep
// re-building the resouces package and reloading the browser. To solve
// this, enable the following flag to read the webapp's source files
// directly off disk, so all you have to do is refresh the page to
// test the modifications.
// #define USE_SOURCE_FILES_DIRECTLY

#include "base/bind.h"
#include "base/debug/debugging_flags.h"
#include "base/debug/thread_heap_usage_tracker.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/tracked_objects.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_profiler/task_profiler_data_serializer.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "components/metrics/profiler/tracking_synchronizer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"

#ifdef USE_SOURCE_FILES_DIRECTLY
#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/path_service.h"
#endif  //  USE_SOURCE_FILES_DIRECTLY

using content::BrowserThread;
using content::WebContents;
using content::WebUIMessageHandler;
using metrics::TrackingSynchronizer;

namespace {

#ifdef USE_SOURCE_FILES_DIRECTLY

class ProfilerWebUIDataSource : public content::URLDataSource {
 public:
  ProfilerWebUIDataSource() {
  }

 protected:
  // content::URLDataSource implementation.
  std::string GetSource() override {
    return chrome::kChromeUIProfilerHost;
  }

  std::string GetMimeType(const std::string& path) const override {
    if (base::EndsWith(path, ".js", base::CompareCase::INSENSITIVE_ASCII))
      return "application/javascript";
    return "text/html";
  }

  void StartDataRequest(
      const std::string& path,
      bool is_incognito,
      const content::URLDataSource::GotDataCallback& callback) override {
    base::FilePath base_path;
    PathService::Get(base::DIR_SOURCE_ROOT, &base_path);
    base_path = base_path.AppendASCII("chrome");
    base_path = base_path.AppendASCII("browser");
    base_path = base_path.AppendASCII("resources");
    base_path = base_path.AppendASCII("profiler");

    // If no resource was specified, default to profiler.html.
    std::string filename = path.empty() ? "profiler.html" : path;

    base::FilePath file_path;
    file_path = base_path.AppendASCII(filename);

    // Read the file synchronously and send it as the response.
    base::ThreadRestrictions::ScopedAllowIO allow;
    std::string file_contents;
    if (!base::ReadFileToString(file_path, &file_contents))
      LOG(ERROR) << "Couldn't read file: " << file_path.value();
    scoped_refptr<base::RefCountedString> response =
        new base::RefCountedString();
    response->data() = file_contents;
    callback.Run(response);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ProfilerWebUIDataSource);
};

#else  // USE_SOURCE_FILES_DIRECTLY

content::WebUIDataSource* CreateProfilerHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIProfilerHost);

  source->SetJsonPath("strings.js");
  source->AddResourcePath("profiler.js", IDR_PROFILER_JS);
  source->SetDefaultResource(IDR_PROFILER_HTML);
  source->UseGzip(std::unordered_set<std::string>());
  source->AddBoolean(
      "enableMemoryTaskProfiler",
      base::debug::ThreadHeapUsageTracker::IsHeapTrackingEnabled());

  return source;
}

#endif

// This class receives javascript messages from the renderer.
// Note that the WebUI infrastructure runs on the UI thread, therefore all of
// this class's methods are expected to run on the UI thread.
class ProfilerMessageHandler : public WebUIMessageHandler {
 public:
  ProfilerMessageHandler() {}

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // Messages.
  void OnGetData(const base::ListValue* list);

 private:
  DISALLOW_COPY_AND_ASSIGN(ProfilerMessageHandler);
};

void ProfilerMessageHandler::RegisterMessages() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  web_ui()->RegisterMessageCallback("getData",
      base::Bind(&ProfilerMessageHandler::OnGetData, base::Unretained(this)));
}

void ProfilerMessageHandler::OnGetData(const base::ListValue* list) {
  ProfilerUI* profiler_ui = static_cast<ProfilerUI*>(web_ui()->GetController());
  profiler_ui->GetData();
}

}  // namespace

ProfilerUI::ProfilerUI(content::WebUI* web_ui)
    : WebUIController(web_ui),
      weak_ptr_factory_(this) {
  web_ui->AddMessageHandler(base::MakeUnique<ProfilerMessageHandler>());

  // Set up the chrome://profiler/ source.
  Profile* profile = Profile::FromWebUI(web_ui);
#if defined(USE_SOURCE_FILES_DIRECTLY)
  content::URLDataSource::Add(profile, new ProfilerWebUIDataSource);
#else
  content::WebUIDataSource::Add(profile, CreateProfilerHTMLSource());
#endif
}

ProfilerUI::~ProfilerUI() {
}

void ProfilerUI::GetData() {
  TrackingSynchronizer::FetchProfilerDataAsynchronously(
      weak_ptr_factory_.GetWeakPtr());
}

void ProfilerUI::ReceivedProfilerData(
    const metrics::ProfilerDataAttributes& attributes,
    const tracked_objects::ProcessDataPhaseSnapshot& process_data_phase,
    const metrics::ProfilerEvents& past_events) {
  // Serialize the data to JSON.
  base::DictionaryValue json_data;
  task_profiler::TaskProfilerDataSerializer::ToValue(
      process_data_phase, attributes.process_id, attributes.process_type,
      &json_data);

  // Send the data to the renderer.
  web_ui()->CallJavascriptFunctionUnsafe("g_browserBridge.receivedData",
                                         json_data);
}
