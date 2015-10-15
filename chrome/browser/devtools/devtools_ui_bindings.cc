// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_ui_bindings.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/devtools/devtools_target_impl.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/chrome_manifest_url_handlers.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/syncable_prefs/pref_service_syncable.h"
#include "components/ui/zoom/page_zoom.h"
#include "content/public/browser/devtools_external_agent_proxy.h"
#include "content/public/browser/devtools_external_agent_proxy_delegate.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/permissions/permissions_data.h"
#include "ipc/ipc_channel.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_response_writer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"

using base::DictionaryValue;
using content::BrowserThread;

namespace content {
struct LoadCommittedDetails;
struct FrameNavigateParams;
}

namespace {

static const char kFrontendHostId[] = "id";
static const char kFrontendHostMethod[] = "method";
static const char kFrontendHostParams[] = "params";
static const char kTitleFormat[] = "Developer Tools - %s";

static const char kDevToolsActionTakenHistogram[] = "DevTools.ActionTaken";
static const char kDevToolsPanelShownHistogram[] = "DevTools.PanelShown";

static const char kRemotePageActionInspect[] = "inspect";
static const char kRemotePageActionReload[] = "reload";
static const char kRemotePageActionActivate[] = "activate";
static const char kRemotePageActionClose[] = "close";

// This constant should be in sync with
// the constant at shell_devtools_frontend.cc.
const size_t kMaxMessageChunkSize = IPC::Channel::kMaximumMessageSize / 4;

typedef std::vector<DevToolsUIBindings*> DevToolsUIBindingsList;
base::LazyInstance<DevToolsUIBindingsList>::Leaky g_instances =
    LAZY_INSTANCE_INITIALIZER;

base::DictionaryValue* CreateFileSystemValue(
    DevToolsFileHelper::FileSystem file_system) {
  base::DictionaryValue* file_system_value = new base::DictionaryValue();
  file_system_value->SetString("fileSystemName", file_system.file_system_name);
  file_system_value->SetString("rootURL", file_system.root_url);
  file_system_value->SetString("fileSystemPath", file_system.file_system_path);
  return file_system_value;
}

Browser* FindBrowser(content::WebContents* web_contents) {
  for (chrome::BrowserIterator it; !it.done(); it.Next()) {
    int tab_index = it->tab_strip_model()->GetIndexOfWebContents(
        web_contents);
    if (tab_index != TabStripModel::kNoTab)
      return *it;
  }
  return NULL;
}

// DevToolsConfirmInfoBarDelegate ---------------------------------------------

typedef base::Callback<void(bool)> InfoBarCallback;

class DevToolsConfirmInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // If |infobar_service| is NULL, runs |callback| with a single argument with
  // value "false".  Otherwise, creates a dev tools confirm infobar and delegate
  // and adds the infobar to |infobar_service|.
  static void Create(InfoBarService* infobar_service,
                     const InfoBarCallback& callback,
                     const base::string16& message);

 private:
  DevToolsConfirmInfoBarDelegate(
      const InfoBarCallback& callback,
      const base::string16& message);
  ~DevToolsConfirmInfoBarDelegate() override;

  base::string16 GetMessageText() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  bool Cancel() override;

  InfoBarCallback callback_;
  const base::string16 message_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsConfirmInfoBarDelegate);
};

void DevToolsConfirmInfoBarDelegate::Create(
    InfoBarService* infobar_service,
    const InfoBarCallback& callback,
    const base::string16& message) {
  if (!infobar_service) {
    callback.Run(false);
    return;
  }

  infobar_service->AddInfoBar(
      infobar_service->CreateConfirmInfoBar(scoped_ptr<ConfirmInfoBarDelegate>(
          new DevToolsConfirmInfoBarDelegate(callback, message))));
}

DevToolsConfirmInfoBarDelegate::DevToolsConfirmInfoBarDelegate(
    const InfoBarCallback& callback,
    const base::string16& message)
    : ConfirmInfoBarDelegate(),
      callback_(callback),
      message_(message) {
}

DevToolsConfirmInfoBarDelegate::~DevToolsConfirmInfoBarDelegate() {
  if (!callback_.is_null())
    callback_.Run(false);
}

base::string16 DevToolsConfirmInfoBarDelegate::GetMessageText() const {
  return message_;
}

base::string16 DevToolsConfirmInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_DEV_TOOLS_CONFIRM_ALLOW_BUTTON : IDS_DEV_TOOLS_CONFIRM_DENY_BUTTON);
}

bool DevToolsConfirmInfoBarDelegate::Accept() {
  callback_.Run(true);
  callback_.Reset();
  return true;
}

bool DevToolsConfirmInfoBarDelegate::Cancel() {
  callback_.Run(false);
  callback_.Reset();
  return true;
}

// DevToolsUIDefaultDelegate --------------------------------------------------

class DefaultBindingsDelegate : public DevToolsUIBindings::Delegate {
 public:
  explicit DefaultBindingsDelegate(content::WebContents* web_contents)
      : web_contents_(web_contents) {}

 private:
  ~DefaultBindingsDelegate() override {}

  void ActivateWindow() override;
  void CloseWindow() override {}
  void SetInspectedPageBounds(const gfx::Rect& rect) override {}
  void InspectElementCompleted() override {}
  void SetIsDocked(bool is_docked) override {}
  void OpenInNewTab(const std::string& url) override;
  void SetWhitelistedShortcuts(const std::string& message) override {}
  using DispatchCallback =
      DevToolsEmbedderMessageDispatcher::Delegate::DispatchCallback;

  void InspectedContentsClosing() override;
  void OnLoadCompleted() override {}
  InfoBarService* GetInfoBarService() override;
  void RenderProcessGone(bool crashed) override {}

  content::WebContents* web_contents_;
  DISALLOW_COPY_AND_ASSIGN(DefaultBindingsDelegate);
};

void DefaultBindingsDelegate::ActivateWindow() {
  web_contents_->GetDelegate()->ActivateContents(web_contents_);
  web_contents_->Focus();
}

void DefaultBindingsDelegate::OpenInNewTab(const std::string& url) {
  content::OpenURLParams params(
      GURL(url), content::Referrer(), NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_LINK, false);
  Browser* browser = FindBrowser(web_contents_);
  browser->OpenURL(params);
}

void DefaultBindingsDelegate::InspectedContentsClosing() {
  web_contents_->ClosePage();
}

InfoBarService* DefaultBindingsDelegate::GetInfoBarService() {
  return InfoBarService::FromWebContents(web_contents_);
}

// ResponseWriter -------------------------------------------------------------

class ResponseWriter : public net::URLFetcherResponseWriter {
 public:
  ResponseWriter(base::WeakPtr<DevToolsUIBindings> bindings, int stream_id);
  ~ResponseWriter() override;

  // URLFetcherResponseWriter overrides:
  int Initialize(const net::CompletionCallback& callback) override;
  int Write(net::IOBuffer* buffer,
            int num_bytes,
            const net::CompletionCallback& callback) override;
  int Finish(const net::CompletionCallback& callback) override;

 private:
  base::WeakPtr<DevToolsUIBindings> bindings_;
  int stream_id_;

  DISALLOW_COPY_AND_ASSIGN(ResponseWriter);
};

ResponseWriter::ResponseWriter(base::WeakPtr<DevToolsUIBindings> bindings,
                               int stream_id)
    : bindings_(bindings),
      stream_id_(stream_id) {
}

ResponseWriter::~ResponseWriter() {
}

int ResponseWriter::Initialize(const net::CompletionCallback& callback) {
  return net::OK;
}

int ResponseWriter::Write(net::IOBuffer* buffer,
                          int num_bytes,
                          const net::CompletionCallback& callback) {
  base::FundamentalValue* id = new base::FundamentalValue(stream_id_);
  base::StringValue* chunk =
      new base::StringValue(std::string(buffer->data(), num_bytes));

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&DevToolsUIBindings::CallClientFunction,
                 bindings_, "DevToolsAPI.streamWrite",
                 base::Owned(id), base::Owned(chunk), nullptr));
  return num_bytes;
}

int ResponseWriter::Finish(const net::CompletionCallback& callback) {
  return net::OK;
}

}  // namespace

// DevToolsUIBindings::FrontendWebContentsObserver ----------------------------

class DevToolsUIBindings::FrontendWebContentsObserver
    : public content::WebContentsObserver {
 public:
  explicit FrontendWebContentsObserver(DevToolsUIBindings* ui_bindings);
  ~FrontendWebContentsObserver() override;

 private:
  // contents::WebContentsObserver:
  void RenderProcessGone(base::TerminationStatus status) override;
  void DidStartNavigationToPendingEntry(
      const GURL& url,
      content::NavigationController::ReloadType reload_type) override;
  void DocumentOnLoadCompletedInMainFrame() override;
  void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) override;

  DevToolsUIBindings* devtools_bindings_;
  DISALLOW_COPY_AND_ASSIGN(FrontendWebContentsObserver);
};

DevToolsUIBindings::FrontendWebContentsObserver::FrontendWebContentsObserver(
    DevToolsUIBindings* devtools_ui_bindings)
    : WebContentsObserver(devtools_ui_bindings->web_contents()),
      devtools_bindings_(devtools_ui_bindings) {
}

DevToolsUIBindings::FrontendWebContentsObserver::
    ~FrontendWebContentsObserver() {
}

void DevToolsUIBindings::FrontendWebContentsObserver::RenderProcessGone(
    base::TerminationStatus status) {
  bool crashed = true;
  switch (status) {
    case base::TERMINATION_STATUS_ABNORMAL_TERMINATION:
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED:
#if defined(OS_CHROMEOS)
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM:
#endif
    case base::TERMINATION_STATUS_PROCESS_CRASHED:
    case base::TERMINATION_STATUS_LAUNCH_FAILED:
      if (devtools_bindings_->agent_host_.get())
        devtools_bindings_->Detach();
      break;
    default:
      crashed = false;
      break;
  }
  devtools_bindings_->delegate_->RenderProcessGone(crashed);
}

void DevToolsUIBindings::FrontendWebContentsObserver::
    DidStartNavigationToPendingEntry(
        const GURL& url,
        content::NavigationController::ReloadType reload_type) {
  devtools_bindings_->frontend_host_.reset(
      content::DevToolsFrontendHost::Create(web_contents()->GetMainFrame(),
                                            devtools_bindings_));
}

void DevToolsUIBindings::FrontendWebContentsObserver::
    DocumentOnLoadCompletedInMainFrame() {
  devtools_bindings_->DocumentOnLoadCompletedInMainFrame();
}

void DevToolsUIBindings::FrontendWebContentsObserver::
    DidNavigateMainFrame(const content::LoadCommittedDetails& details,
                         const content::FrameNavigateParams& params) {
  devtools_bindings_->DidNavigateMainFrame();
}

// WebSocketAPIChannel --------------------------------------------------------

class DevToolsUIBindings::WebSocketAPIChannel :
    public content::DevToolsExternalAgentProxyDelegate {
 public :
  explicit WebSocketAPIChannel(base::WeakPtr<DevToolsUIBindings> bindings);
  ~WebSocketAPIChannel() override;
  void DispatchOnClientHost(const std::string& message);
  void ConnectionClosed();

 private:
  // content::DevToolsExternalAgentProxyDelegate implementation.
  void Attach(content::DevToolsExternalAgentProxy* proxy) override;
  void Detach() override;
  void SendMessageToBackend(const std::string& message) override;

  base::WeakPtr<DevToolsUIBindings> bindings_;
  content::DevToolsExternalAgentProxy* attached_proxy_;
  DISALLOW_COPY_AND_ASSIGN(WebSocketAPIChannel);
};

DevToolsUIBindings::WebSocketAPIChannel::WebSocketAPIChannel(
    base::WeakPtr<DevToolsUIBindings> bindings)
    : bindings_(bindings),
      attached_proxy_(nullptr) {
  bindings->open_api_channel_ = this;
}

DevToolsUIBindings::WebSocketAPIChannel::~WebSocketAPIChannel() {
  if (bindings_)
    bindings_->open_api_channel_ = nullptr;
}

void DevToolsUIBindings::WebSocketAPIChannel::DispatchOnClientHost(
    const std::string& message) {
  if (attached_proxy_)
    attached_proxy_->DispatchOnClientHost(message);
}

void DevToolsUIBindings::WebSocketAPIChannel::ConnectionClosed() {
  if (bindings_) {
    bindings_->CallClientFunction("DevToolsAPI.frontendAPIDetached",
                                  nullptr, nullptr, nullptr);
  }
  if (attached_proxy_)
    attached_proxy_->ConnectionClosed();
}

void DevToolsUIBindings::WebSocketAPIChannel::Attach(
    content::DevToolsExternalAgentProxy* proxy) {
  attached_proxy_ = proxy;
  if (bindings_) {
    bindings_->CallClientFunction("DevToolsAPI.frontendAPIAttached",
                                  nullptr, nullptr, nullptr);
  }
}

void DevToolsUIBindings::WebSocketAPIChannel::Detach() {
  attached_proxy_ = nullptr;
  if (bindings_) {
    bindings_->open_api_channel_ = nullptr;
    bindings_->CallClientFunction("DevToolsAPI.frontendAPIDetached",
                                  nullptr, nullptr, nullptr);
  }
}

void DevToolsUIBindings::WebSocketAPIChannel::SendMessageToBackend(
    const std::string& message) {
  if (!bindings_)
    return;
  base::StringValue message_value(message);
  bindings_->CallClientFunction("DevToolsAPI.dispatchFrontendAPIMessage",
                                &message_value, nullptr, nullptr);
}

// DevToolsUIBindings ---------------------------------------------------------

DevToolsUIBindings* DevToolsUIBindings::ForWebContents(
     content::WebContents* web_contents) {
 if (g_instances == NULL)
   return NULL;
 DevToolsUIBindingsList* instances = g_instances.Pointer();
 for (DevToolsUIBindingsList::iterator it(instances->begin());
      it != instances->end(); ++it) {
   if ((*it)->web_contents() == web_contents)
     return *it;
 }
 return NULL;
}

DevToolsUIBindings::DevToolsUIBindings(content::WebContents* web_contents)
    : profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      android_bridge_(DevToolsAndroidBridge::Factory::GetForProfile(profile_)),
      web_contents_(web_contents),
      delegate_(new DefaultBindingsDelegate(web_contents_)),
      devices_updates_enabled_(false),
      frontend_loaded_(false),
      open_api_channel_(nullptr),
      weak_factory_(this) {
  g_instances.Get().push_back(this);
  frontend_contents_observer_.reset(new FrontendWebContentsObserver(this));
  web_contents_->GetMutableRendererPrefs()->can_accept_load_drops = false;

  file_helper_.reset(new DevToolsFileHelper(web_contents_, profile_, this));
  file_system_indexer_ = new DevToolsFileSystemIndexer();
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      web_contents_);

  // Register on-load actions.
  embedder_message_dispatcher_.reset(
      DevToolsEmbedderMessageDispatcher::CreateForDevToolsFrontend(this));

  frontend_host_.reset(content::DevToolsFrontendHost::Create(
      web_contents_->GetMainFrame(), this));
}

DevToolsUIBindings::~DevToolsUIBindings() {
  for (const auto& pair : pending_requests_)
    delete pair.first;

  if (agent_host_.get())
    agent_host_->DetachClient();

  for (IndexingJobsMap::const_iterator jobs_it(indexing_jobs_.begin());
       jobs_it != indexing_jobs_.end(); ++jobs_it) {
    jobs_it->second->Stop();
  }
  indexing_jobs_.clear();
  SetDevicesUpdatesEnabled(false);

  if (open_api_channel_)
    open_api_channel_->ConnectionClosed();

  // Remove self from global list.
  DevToolsUIBindingsList* instances = g_instances.Pointer();
  DevToolsUIBindingsList::iterator it(
      std::find(instances->begin(), instances->end(), this));
  DCHECK(it != instances->end());
  instances->erase(it);
}

// content::DevToolsFrontendHost::Delegate implementation ---------------------
void DevToolsUIBindings::HandleMessageFromDevToolsFrontend(
    const std::string& message) {
  std::string method;
  base::ListValue empty_params;
  base::ListValue* params = &empty_params;

  base::DictionaryValue* dict = NULL;
  scoped_ptr<base::Value> parsed_message = base::JSONReader::Read(message);
  if (!parsed_message ||
      !parsed_message->GetAsDictionary(&dict) ||
      !dict->GetString(kFrontendHostMethod, &method) ||
      (dict->HasKey(kFrontendHostParams) &&
          !dict->GetList(kFrontendHostParams, &params))) {
    LOG(ERROR) << "Invalid message was sent to embedder: " << message;
    return;
  }
  int id = 0;
  dict->GetInteger(kFrontendHostId, &id);
  embedder_message_dispatcher_->Dispatch(
      base::Bind(&DevToolsUIBindings::SendMessageAck,
                 weak_factory_.GetWeakPtr(),
                 id),
      method,
      params);
}

void DevToolsUIBindings::HandleMessageFromDevToolsFrontendToBackend(
    const std::string& message) {
  if (agent_host_.get())
    agent_host_->DispatchProtocolMessage(message);
}

// content::DevToolsAgentHostClient implementation --------------------------
void DevToolsUIBindings::DispatchProtocolMessage(
    content::DevToolsAgentHost* agent_host, const std::string& message) {
  DCHECK(agent_host == agent_host_.get());

  if (message.length() < kMaxMessageChunkSize) {
    base::string16 javascript = base::UTF8ToUTF16(
        "DevToolsAPI.dispatchMessage(" + message + ");");
    web_contents_->GetMainFrame()->ExecuteJavaScript(javascript);
    return;
  }

  base::FundamentalValue total_size(static_cast<int>(message.length()));
  for (size_t pos = 0; pos < message.length(); pos += kMaxMessageChunkSize) {
    base::StringValue message_value(message.substr(pos, kMaxMessageChunkSize));
    CallClientFunction("DevToolsAPI.dispatchMessageChunk",
                       &message_value, pos ? NULL : &total_size, NULL);
  }
}

void DevToolsUIBindings::AgentHostClosed(
    content::DevToolsAgentHost* agent_host,
    bool replaced_with_another_client) {
  DCHECK(agent_host == agent_host_.get());
  agent_host_ = NULL;
  delegate_->InspectedContentsClosing();
}

void DevToolsUIBindings::SendMessageAck(int request_id,
                                        const base::Value* arg) {
  base::FundamentalValue id_value(request_id);
  CallClientFunction("DevToolsAPI.embedderMessageAck",
                     &id_value, arg, nullptr);
}

// DevToolsEmbedderMessageDispatcher::Delegate implementation -----------------
void DevToolsUIBindings::ActivateWindow() {
  delegate_->ActivateWindow();
}

void DevToolsUIBindings::CloseWindow() {
  delegate_->CloseWindow();
}

void DevToolsUIBindings::LoadCompleted() {
  FrontendLoaded();
}

void DevToolsUIBindings::SetInspectedPageBounds(const gfx::Rect& rect) {
  delegate_->SetInspectedPageBounds(rect);
}

void DevToolsUIBindings::SetIsDocked(const DispatchCallback& callback,
                                     bool dock_requested) {
  delegate_->SetIsDocked(dock_requested);
  callback.Run(nullptr);
}

void DevToolsUIBindings::InspectElementCompleted() {
  delegate_->InspectElementCompleted();
}

void DevToolsUIBindings::InspectedURLChanged(const std::string& url) {
  content::NavigationController& controller = web_contents()->GetController();
  content::NavigationEntry* entry = controller.GetActiveEntry();
  // DevTools UI is not localized.
  entry->SetTitle(
      base::UTF8ToUTF16(base::StringPrintf(kTitleFormat, url.c_str())));
  web_contents()->NotifyNavigationStateChanged(content::INVALIDATE_TYPE_TITLE);
}

void DevToolsUIBindings::LoadNetworkResource(const DispatchCallback& callback,
                                             const std::string& url,
                                             const std::string& headers,
                                             int stream_id) {
  GURL gurl(url);
  if (!gurl.is_valid()) {
    base::DictionaryValue response;
    response.SetInteger("statusCode", 404);
    callback.Run(&response);
    return;
  }

  net::URLFetcher* fetcher =
      net::URLFetcher::Create(gurl, net::URLFetcher::GET, this).release();
  pending_requests_[fetcher] = callback;
  fetcher->SetRequestContext(profile_->GetRequestContext());
  fetcher->SetExtraRequestHeaders(headers);
  fetcher->SaveResponseWithWriter(scoped_ptr<net::URLFetcherResponseWriter>(
      new ResponseWriter(weak_factory_.GetWeakPtr(), stream_id)));
  fetcher->Start();
}

void DevToolsUIBindings::OpenInNewTab(const std::string& url) {
  delegate_->OpenInNewTab(url);
}

void DevToolsUIBindings::SaveToFile(const std::string& url,
                                    const std::string& content,
                                    bool save_as) {
  file_helper_->Save(url, content, save_as,
                     base::Bind(&DevToolsUIBindings::FileSavedAs,
                                weak_factory_.GetWeakPtr(), url),
                     base::Bind(&DevToolsUIBindings::CanceledFileSaveAs,
                                weak_factory_.GetWeakPtr(), url));
}

void DevToolsUIBindings::AppendToFile(const std::string& url,
                                      const std::string& content) {
  file_helper_->Append(url, content,
                       base::Bind(&DevToolsUIBindings::AppendedTo,
                                  weak_factory_.GetWeakPtr(), url));
}

void DevToolsUIBindings::RequestFileSystems() {
  CHECK(web_contents_->GetURL().SchemeIs(content::kChromeDevToolsScheme));
  std::vector<DevToolsFileHelper::FileSystem> file_systems =
      file_helper_->GetFileSystems();
  base::ListValue file_systems_value;
  for (size_t i = 0; i < file_systems.size(); ++i)
    file_systems_value.Append(CreateFileSystemValue(file_systems[i]));
  CallClientFunction("DevToolsAPI.fileSystemsLoaded",
                     &file_systems_value, NULL, NULL);
}

void DevToolsUIBindings::AddFileSystem(const std::string& file_system_path) {
  CHECK(web_contents_->GetURL().SchemeIs(content::kChromeDevToolsScheme));
  file_helper_->AddFileSystem(
      file_system_path,
      base::Bind(&DevToolsUIBindings::ShowDevToolsConfirmInfoBar,
                 weak_factory_.GetWeakPtr()));
}

void DevToolsUIBindings::RemoveFileSystem(const std::string& file_system_path) {
  CHECK(web_contents_->GetURL().SchemeIs(content::kChromeDevToolsScheme));
  file_helper_->RemoveFileSystem(file_system_path);
}

void DevToolsUIBindings::UpgradeDraggedFileSystemPermissions(
    const std::string& file_system_url) {
  CHECK(web_contents_->GetURL().SchemeIs(content::kChromeDevToolsScheme));
  file_helper_->UpgradeDraggedFileSystemPermissions(
      file_system_url,
      base::Bind(&DevToolsUIBindings::ShowDevToolsConfirmInfoBar,
                 weak_factory_.GetWeakPtr()));
}

void DevToolsUIBindings::IndexPath(int index_request_id,
                                   const std::string& file_system_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(web_contents_->GetURL().SchemeIs(content::kChromeDevToolsScheme));
  if (!file_helper_->IsFileSystemAdded(file_system_path)) {
    IndexingDone(index_request_id, file_system_path);
    return;
  }
  if (indexing_jobs_.count(index_request_id) != 0)
    return;
  indexing_jobs_[index_request_id] =
      scoped_refptr<DevToolsFileSystemIndexer::FileSystemIndexingJob>(
          file_system_indexer_->IndexPath(
              file_system_path,
              Bind(&DevToolsUIBindings::IndexingTotalWorkCalculated,
                   weak_factory_.GetWeakPtr(),
                   index_request_id,
                   file_system_path),
              Bind(&DevToolsUIBindings::IndexingWorked,
                   weak_factory_.GetWeakPtr(),
                   index_request_id,
                   file_system_path),
              Bind(&DevToolsUIBindings::IndexingDone,
                   weak_factory_.GetWeakPtr(),
                   index_request_id,
                   file_system_path)));
}

void DevToolsUIBindings::StopIndexing(int index_request_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  IndexingJobsMap::iterator it = indexing_jobs_.find(index_request_id);
  if (it == indexing_jobs_.end())
    return;
  it->second->Stop();
  indexing_jobs_.erase(it);
}

void DevToolsUIBindings::SearchInPath(int search_request_id,
                                      const std::string& file_system_path,
                                      const std::string& query) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(web_contents_->GetURL().SchemeIs(content::kChromeDevToolsScheme));
  if (!file_helper_->IsFileSystemAdded(file_system_path)) {
    SearchCompleted(search_request_id,
                    file_system_path,
                    std::vector<std::string>());
    return;
  }
  file_system_indexer_->SearchInPath(file_system_path,
                                     query,
                                     Bind(&DevToolsUIBindings::SearchCompleted,
                                          weak_factory_.GetWeakPtr(),
                                          search_request_id,
                                          file_system_path));
}

void DevToolsUIBindings::SetWhitelistedShortcuts(const std::string& message) {
  delegate_->SetWhitelistedShortcuts(message);
}

void DevToolsUIBindings::ZoomIn() {
  ui_zoom::PageZoom::Zoom(web_contents(), content::PAGE_ZOOM_IN);
}

void DevToolsUIBindings::ZoomOut() {
  ui_zoom::PageZoom::Zoom(web_contents(), content::PAGE_ZOOM_OUT);
}

void DevToolsUIBindings::ResetZoom() {
  ui_zoom::PageZoom::Zoom(web_contents(), content::PAGE_ZOOM_RESET);
}

void DevToolsUIBindings::SetDevicesDiscoveryConfig(
    bool discover_usb_devices,
    bool port_forwarding_enabled,
    const std::string& port_forwarding_config) {
  base::DictionaryValue* config_dict = nullptr;
  scoped_ptr<base::Value> parsed_config =
      base::JSONReader::Read(port_forwarding_config);
  if (!parsed_config || !parsed_config->GetAsDictionary(&config_dict))
    return;

  profile_->GetPrefs()->SetBoolean(
      prefs::kDevToolsDiscoverUsbDevicesEnabled, discover_usb_devices);
  profile_->GetPrefs()->SetBoolean(
      prefs::kDevToolsPortForwardingEnabled, port_forwarding_enabled);
  profile_->GetPrefs()->Set(
      prefs::kDevToolsPortForwardingConfig, *config_dict);
}

void DevToolsUIBindings::DevicesDiscoveryConfigUpdated() {
  CallClientFunction(
      "DevToolsAPI.devicesDiscoveryConfigChanged",
      profile_->GetPrefs()->FindPreference(
          prefs::kDevToolsDiscoverUsbDevicesEnabled)->GetValue(),
      profile_->GetPrefs()->FindPreference(
          prefs::kDevToolsPortForwardingEnabled)->GetValue(),
      profile_->GetPrefs()->FindPreference(
          prefs::kDevToolsPortForwardingConfig)->GetValue());
}

void DevToolsUIBindings::SetDevicesUpdatesEnabled(bool enabled) {
  if (devices_updates_enabled_ == enabled)
    return;
  devices_updates_enabled_ = enabled;
  if (enabled) {
    remote_targets_handler_ = DevToolsTargetsUIHandler::CreateForAdb(
        base::Bind(&DevToolsUIBindings::DevicesUpdated,
                   base::Unretained(this)),
        profile_);
    pref_change_registrar_.Init(profile_->GetPrefs());
    pref_change_registrar_.Add(prefs::kDevToolsDiscoverUsbDevicesEnabled,
        base::Bind(&DevToolsUIBindings::DevicesDiscoveryConfigUpdated,
                   base::Unretained(this)));
    pref_change_registrar_.Add(prefs::kDevToolsPortForwardingEnabled,
        base::Bind(&DevToolsUIBindings::DevicesDiscoveryConfigUpdated,
                   base::Unretained(this)));
    pref_change_registrar_.Add(prefs::kDevToolsPortForwardingConfig,
        base::Bind(&DevToolsUIBindings::DevicesDiscoveryConfigUpdated,
                   base::Unretained(this)));
    DevicesDiscoveryConfigUpdated();
  } else {
    remote_targets_handler_.reset();
    pref_change_registrar_.RemoveAll();
  }
}

void DevToolsUIBindings::PerformActionOnRemotePage(const std::string& page_id,
                                                   const std::string& action) {
  if (!remote_targets_handler_)
    return;
  DevToolsTargetImpl* target = remote_targets_handler_->GetTarget(page_id);
  if (!target)
    return;
  if (action == kRemotePageActionInspect)
    target->Inspect(profile_);
  if (action == kRemotePageActionReload)
    target->Reload();
  if (action == kRemotePageActionActivate)
    target->Activate();
  if (action == kRemotePageActionClose)
    target->Close();
}

void DevToolsUIBindings::GetPreferences(const DispatchCallback& callback) {
  const DictionaryValue* prefs =
      profile_->GetPrefs()->GetDictionary(prefs::kDevToolsPreferences);
  callback.Run(prefs);
}

void DevToolsUIBindings::SetPreference(const std::string& name,
                                   const std::string& value) {
  DictionaryPrefUpdate update(profile_->GetPrefs(),
                              prefs::kDevToolsPreferences);
  update.Get()->SetStringWithoutPathExpansion(name, value);
}

void DevToolsUIBindings::RemovePreference(const std::string& name) {
  DictionaryPrefUpdate update(profile_->GetPrefs(),
                              prefs::kDevToolsPreferences);
  update.Get()->RemoveWithoutPathExpansion(name, nullptr);
}

void DevToolsUIBindings::ClearPreferences() {
  DictionaryPrefUpdate update(profile_->GetPrefs(),
                              prefs::kDevToolsPreferences);
  update.Get()->Clear();
}

void DevToolsUIBindings::SendMessageToBrowser(const std::string& message) {
  if (agent_host_.get())
    agent_host_->DispatchProtocolMessage(message);
}

void DevToolsUIBindings::RecordEnumeratedHistogram(const std::string& name,
                                                   int sample,
                                                   int boundary_value) {
  if (!(boundary_value >= 0 && boundary_value <= 100 && sample >= 0 &&
        sample < boundary_value)) {
    // TODO(nick): Replace with chrome::bad_message::ReceivedBadMessage().
    frontend_host_->BadMessageRecieved();
    return;
  }
  // Each histogram name must follow a different code path in
  // order to UMA_HISTOGRAM_ENUMERATION work correctly.
  if (name == kDevToolsActionTakenHistogram)
    UMA_HISTOGRAM_ENUMERATION(name, sample, boundary_value);
  else if (name == kDevToolsPanelShownHistogram)
    UMA_HISTOGRAM_ENUMERATION(name, sample, boundary_value);
  else
    frontend_host_->BadMessageRecieved();
}

void DevToolsUIBindings::SendJsonRequest(const DispatchCallback& callback,
                                         const std::string& browser_id,
                                         const std::string& url) {
  if (!android_bridge_) {
    callback.Run(nullptr);
    return;
  }
  android_bridge_->SendJsonRequest(browser_id, url,
      base::Bind(&DevToolsUIBindings::JsonReceived,
                 weak_factory_.GetWeakPtr(),
                 callback));
}

void DevToolsUIBindings::SendFrontendAPINotification(
    const std::string& message) {
  if (!open_api_channel_)
    return;
  open_api_channel_->DispatchOnClientHost(message);
}

void DevToolsUIBindings::JsonReceived(const DispatchCallback& callback,
                                      int result,
                                      const std::string& message) {
  if (result != net::OK) {
    callback.Run(nullptr);
    return;
  }
  base::StringValue message_value(message);
  callback.Run(&message_value);
}

void DevToolsUIBindings::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK(source);
  PendingRequestsMap::iterator it = pending_requests_.find(source);
  DCHECK(it != pending_requests_.end());

  base::DictionaryValue response;
  base::DictionaryValue* headers = new base::DictionaryValue();
  net::HttpResponseHeaders* rh = source->GetResponseHeaders();
  response.SetInteger("statusCode", rh ? rh->response_code() : 200);
  response.Set("headers", headers);

  void* iterator = NULL;
  std::string name;
  std::string value;
  while (rh && rh->EnumerateHeaderLines(&iterator, &name, &value))
    headers->SetString(name, value);

  it->second.Run(&response);
  pending_requests_.erase(it);
  delete source;
}

void DevToolsUIBindings::DeviceCountChanged(int count) {
  base::FundamentalValue value(count);
  CallClientFunction("DevToolsAPI.deviceCountUpdated", &value, NULL,
                     NULL);
}

void DevToolsUIBindings::DevicesUpdated(
    const std::string& source,
    const base::ListValue& targets) {
  CallClientFunction("DevToolsAPI.devicesUpdated", &targets, NULL,
                     NULL);
}

void DevToolsUIBindings::FileSavedAs(const std::string& url) {
  base::StringValue url_value(url);
  CallClientFunction("DevToolsAPI.savedURL", &url_value, NULL, NULL);
}

void DevToolsUIBindings::CanceledFileSaveAs(const std::string& url) {
  base::StringValue url_value(url);
  CallClientFunction("DevToolsAPI.canceledSaveURL",
                     &url_value, NULL, NULL);
}

void DevToolsUIBindings::AppendedTo(const std::string& url) {
  base::StringValue url_value(url);
  CallClientFunction("DevToolsAPI.appendedToURL", &url_value, NULL,
                     NULL);
}

void DevToolsUIBindings::FileSystemAdded(
    const DevToolsFileHelper::FileSystem& file_system) {
  scoped_ptr<base::DictionaryValue> file_system_value(
      CreateFileSystemValue(file_system));
  CallClientFunction("DevToolsAPI.fileSystemAdded",
                     file_system_value.get(), NULL, NULL);
}

void DevToolsUIBindings::FileSystemRemoved(
    const std::string& file_system_path) {
  base::StringValue file_system_path_value(file_system_path);
  CallClientFunction("DevToolsAPI.fileSystemRemoved",
                     &file_system_path_value, NULL, NULL);
}

void DevToolsUIBindings::IndexingTotalWorkCalculated(
    int request_id,
    const std::string& file_system_path,
    int total_work) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::FundamentalValue request_id_value(request_id);
  base::StringValue file_system_path_value(file_system_path);
  base::FundamentalValue total_work_value(total_work);
  CallClientFunction("DevToolsAPI.indexingTotalWorkCalculated",
                     &request_id_value, &file_system_path_value,
                     &total_work_value);
}

void DevToolsUIBindings::IndexingWorked(int request_id,
                                        const std::string& file_system_path,
                                        int worked) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::FundamentalValue request_id_value(request_id);
  base::StringValue file_system_path_value(file_system_path);
  base::FundamentalValue worked_value(worked);
  CallClientFunction("DevToolsAPI.indexingWorked", &request_id_value,
                     &file_system_path_value, &worked_value);
}

void DevToolsUIBindings::IndexingDone(int request_id,
                                      const std::string& file_system_path) {
  indexing_jobs_.erase(request_id);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::FundamentalValue request_id_value(request_id);
  base::StringValue file_system_path_value(file_system_path);
  CallClientFunction("DevToolsAPI.indexingDone", &request_id_value,
                     &file_system_path_value, NULL);
}

void DevToolsUIBindings::SearchCompleted(
    int request_id,
    const std::string& file_system_path,
    const std::vector<std::string>& file_paths) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::ListValue file_paths_value;
  for (std::vector<std::string>::const_iterator it(file_paths.begin());
       it != file_paths.end(); ++it) {
    file_paths_value.AppendString(*it);
  }
  base::FundamentalValue request_id_value(request_id);
  base::StringValue file_system_path_value(file_system_path);
  CallClientFunction("DevToolsAPI.searchCompleted", &request_id_value,
                     &file_system_path_value, &file_paths_value);
}

void DevToolsUIBindings::ShowDevToolsConfirmInfoBar(
    const base::string16& message,
    const InfoBarCallback& callback) {
  DevToolsConfirmInfoBarDelegate::Create(delegate_->GetInfoBarService(),
      callback, message);
}

void DevToolsUIBindings::AddDevToolsExtensionsToClient() {
  const extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile_->GetOriginalProfile());
  if (!registry)
    return;

  base::ListValue results;
  for (const scoped_refptr<const extensions::Extension>& extension :
       registry->enabled_extensions()) {
    if (extensions::chrome_manifest_urls::GetDevToolsPage(extension.get())
            .is_empty())
      continue;
    base::DictionaryValue* extension_info = new base::DictionaryValue();
    extension_info->Set(
        "startPage",
        new base::StringValue(extensions::chrome_manifest_urls::GetDevToolsPage(
                                  extension.get()).spec()));
    extension_info->Set("name", new base::StringValue(extension->name()));
    extension_info->Set("exposeExperimentalAPIs",
                        new base::FundamentalValue(
                            extension->permissions_data()->HasAPIPermission(
                                extensions::APIPermission::kExperimental)));
    results.Append(extension_info);
  }
  CallClientFunction("DevToolsAPI.addExtensions",
                     &results, NULL, NULL);
}

void DevToolsUIBindings::SetDelegate(Delegate* delegate) {
  delegate_.reset(delegate);
}

void DevToolsUIBindings::AttachTo(
    const scoped_refptr<content::DevToolsAgentHost>& agent_host) {
  if (agent_host_.get())
    Detach();
  agent_host_ = agent_host;
  agent_host_->AttachClient(this);
}

void DevToolsUIBindings::Reattach() {
  DCHECK(agent_host_.get());
  agent_host_->DetachClient();
  agent_host_->AttachClient(this);
}

void DevToolsUIBindings::Detach() {
  if (agent_host_.get())
    agent_host_->DetachClient();
  agent_host_ = NULL;
}

bool DevToolsUIBindings::IsAttachedTo(content::DevToolsAgentHost* agent_host) {
  return agent_host_.get() == agent_host;
}

content::DevToolsExternalAgentProxyDelegate*
DevToolsUIBindings::CreateWebSocketAPIChannel() {
  if (open_api_channel_)
    open_api_channel_->ConnectionClosed();
  return new WebSocketAPIChannel(weak_factory_.GetWeakPtr());
}

void DevToolsUIBindings::CallClientFunction(const std::string& function_name,
                                            const base::Value* arg1,
                                            const base::Value* arg2,
                                            const base::Value* arg3) {
  std::string javascript = function_name + "(";
  if (arg1) {
    std::string json;
    base::JSONWriter::Write(*arg1, &json);
    javascript.append(json);
    if (arg2) {
      base::JSONWriter::Write(*arg2, &json);
      javascript.append(", ").append(json);
      if (arg3) {
        base::JSONWriter::Write(*arg3, &json);
        javascript.append(", ").append(json);
      }
    }
  }
  javascript.append(");");
  web_contents_->GetMainFrame()->ExecuteJavaScript(
      base::UTF8ToUTF16(javascript));
}

void DevToolsUIBindings::DocumentOnLoadCompletedInMainFrame() {
  // In the DEBUG_DEVTOOLS mode, the DocumentOnLoadCompletedInMainFrame event
  // arrives before the LoadCompleted event, thus it should not trigger the
  // frontend load handling.
#if !defined(DEBUG_DEVTOOLS)
  FrontendLoaded();
#endif
}

void DevToolsUIBindings::DidNavigateMainFrame() {
  frontend_loaded_ = false;
}

void DevToolsUIBindings::FrontendLoaded() {
  if (frontend_loaded_)
    return;
  frontend_loaded_ = true;

  // Call delegate first - it seeds importants bit of information.
  delegate_->OnLoadCompleted();

  AddDevToolsExtensionsToClient();
}
