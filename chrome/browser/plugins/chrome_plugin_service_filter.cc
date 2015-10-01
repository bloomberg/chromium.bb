// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/chrome_plugin_service_filter.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/plugins/plugin_finder.h"
#include "chrome/browser/plugins/plugin_metadata.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/render_messages.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/content/common/content_settings_messages.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "grit/components_strings.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition.h"

using base::UserMetricsAction;
using content::BrowserThread;
using content::PluginService;

namespace {

// This enum is recorded in a histogram so entries should not be re-ordered or
// removed.
enum PluginGroup {
  GROUP_NAME_UNKNOWN,
  GROUP_NAME_ADOBE_READER,
  GROUP_NAME_JAVA,
  GROUP_NAME_QUICKTIME,
  GROUP_NAME_SHOCKWAVE,
  GROUP_NAME_REALPLAYER,
  GROUP_NAME_SILVERLIGHT,
  GROUP_NAME_WINDOWS_MEDIA_PLAYER,
  GROUP_NAME_GOOGLE_TALK,
  GROUP_NAME_GOOGLE_EARTH,
  GROUP_NAME_COUNT,
};

static const char kLearnMoreUrl[] =
    "https://support.google.com/chrome/answer/6213033";

void AuthorizeRenderer(content::RenderFrameHost* render_frame_host) {
  ChromePluginServiceFilter::GetInstance()->AuthorizePlugin(
      render_frame_host->GetProcess()->GetID(), base::FilePath());
}

class NPAPIRemovalInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  static void Create(InfoBarService* infobar_service,
                     const base::string16& plugin_name,
                     bool is_removed);

 private:
  NPAPIRemovalInfoBarDelegate(const base::string16& plugin_name,
                              int message_id);
  ~NPAPIRemovalInfoBarDelegate() override;

  // ConfirmInfobarDelegate:
  int GetIconId() const override;
  base::string16 GetMessageText() const override;
  int GetButtons() const override;
  base::string16 GetLinkText() const override;
  GURL GetLinkURL() const override;
  bool LinkClicked(WindowOpenDisposition disposition) override;

  base::string16 plugin_name_;
  int message_id_;
};

// static
void NPAPIRemovalInfoBarDelegate::Create(InfoBarService* infobar_service,
                                         const base::string16& plugin_name,
                                         bool is_removed) {
  int message_id = is_removed ? IDS_PLUGINS_NPAPI_REMOVED
                              : IDS_PLUGINS_NPAPI_BEING_REMOVED_SOON;

  infobar_service->AddInfoBar(
      infobar_service->CreateConfirmInfoBar(scoped_ptr<ConfirmInfoBarDelegate>(
          new NPAPIRemovalInfoBarDelegate(plugin_name, message_id))));
}

NPAPIRemovalInfoBarDelegate::NPAPIRemovalInfoBarDelegate(
    const base::string16& plugin_name,
    int message_id)
    : plugin_name_(plugin_name), message_id_(message_id) {
  content::RecordAction(UserMetricsAction("NPAPIRemovalInfobar.Shown"));

  std::pair<PluginGroup, const char*> types[] = {
      std::make_pair(GROUP_NAME_ADOBE_READER,
                     PluginMetadata::kAdobeReaderGroupName),
      std::make_pair(GROUP_NAME_JAVA,
                     PluginMetadata::kJavaGroupName),
      std::make_pair(GROUP_NAME_QUICKTIME,
                     PluginMetadata::kQuickTimeGroupName),
      std::make_pair(GROUP_NAME_SHOCKWAVE,
                     PluginMetadata::kShockwaveGroupName),
      std::make_pair(GROUP_NAME_REALPLAYER,
                     PluginMetadata::kRealPlayerGroupName),
      std::make_pair(GROUP_NAME_SILVERLIGHT,
                     PluginMetadata::kSilverlightGroupName),
      std::make_pair(GROUP_NAME_WINDOWS_MEDIA_PLAYER,
                     PluginMetadata::kWindowsMediaPlayerGroupName),
      std::make_pair(GROUP_NAME_GOOGLE_TALK,
                     PluginMetadata::kGoogleTalkGroupName),
      std::make_pair(GROUP_NAME_GOOGLE_EARTH,
                     PluginMetadata::kGoogleEarthGroupName)};

  PluginGroup group = GROUP_NAME_UNKNOWN;
  std::string name = base::UTF16ToUTF8(plugin_name);

  for (const auto& type : types) {
    if (name == type.second) {
      group = type.first;
      break;
    }
  }

  if (message_id == IDS_PLUGINS_NPAPI_REMOVED) {
    UMA_HISTOGRAM_ENUMERATION(
        "Plugin.NpapiRemovalInfobar.Removed.PluginGroup", group,
        GROUP_NAME_COUNT);
  } else {
    DCHECK_EQ(IDS_PLUGINS_NPAPI_BEING_REMOVED_SOON, message_id);
    UMA_HISTOGRAM_ENUMERATION(
        "Plugin.NpapiRemovalInfobar.RemovedSoon.PluginGroup", group,
        GROUP_NAME_COUNT);
  }
}

NPAPIRemovalInfoBarDelegate::~NPAPIRemovalInfoBarDelegate() {
}

int NPAPIRemovalInfoBarDelegate::GetIconId() const {
  return IDR_INFOBAR_WARNING;
}

base::string16 NPAPIRemovalInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16(message_id_, plugin_name_);
}

int NPAPIRemovalInfoBarDelegate::GetButtons() const {
  return BUTTON_NONE;
}

base::string16 NPAPIRemovalInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

GURL NPAPIRemovalInfoBarDelegate::GetLinkURL() const {
  return GURL(kLearnMoreUrl);
}

bool NPAPIRemovalInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  content::RecordAction(UserMetricsAction("NPAPIRemovalInfobar.LearnMore"));
  ConfirmInfoBarDelegate::LinkClicked(disposition);
  return true;
}

}  // namespace

// static
ChromePluginServiceFilter* ChromePluginServiceFilter::GetInstance() {
  return base::Singleton<ChromePluginServiceFilter>::get();
}

void ChromePluginServiceFilter::RegisterResourceContext(
    PluginPrefs* plugin_prefs,
    const void* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::AutoLock lock(lock_);
  resource_context_map_[context] = plugin_prefs;
}

void ChromePluginServiceFilter::UnregisterResourceContext(
    const void* context) {
  base::AutoLock lock(lock_);
  resource_context_map_.erase(context);
}

void ChromePluginServiceFilter::OverridePluginForFrame(
    int render_process_id,
    int render_frame_id,
    const GURL& url,
    const content::WebPluginInfo& plugin) {
  base::AutoLock auto_lock(lock_);
  ProcessDetails* details = GetOrRegisterProcess(render_process_id);
  OverriddenPlugin overridden_plugin;
  overridden_plugin.render_frame_id = render_frame_id;
  overridden_plugin.url = url;
  overridden_plugin.plugin = plugin;
  details->overridden_plugins.push_back(overridden_plugin);
}

void ChromePluginServiceFilter::RestrictPluginToProfileAndOrigin(
    const base::FilePath& plugin_path,
    Profile* profile,
    const GURL& origin) {
  base::AutoLock auto_lock(lock_);
  restricted_plugins_[plugin_path] =
      std::make_pair(PluginPrefs::GetForProfile(profile).get(), origin);
}

void ChromePluginServiceFilter::UnrestrictPlugin(
    const base::FilePath& plugin_path) {
  base::AutoLock auto_lock(lock_);
  restricted_plugins_.erase(plugin_path);
}

bool ChromePluginServiceFilter::IsPluginRestricted(
    const base::FilePath& plugin_path) {
  base::AutoLock auto_lock(lock_);
  return restricted_plugins_.find(plugin_path) != restricted_plugins_.end();
}

bool ChromePluginServiceFilter::IsPluginAvailable(
    int render_process_id,
    int render_frame_id,
    const void* context,
    const GURL& url,
    const GURL& policy_url,
    content::WebPluginInfo* plugin) {
  base::AutoLock auto_lock(lock_);
  const ProcessDetails* details = GetProcess(render_process_id);

  // Check whether the plugin is overridden.
  if (details) {
    for (const auto& plugin_override : details->overridden_plugins) {
      if (plugin_override.render_frame_id == render_frame_id &&
          (plugin_override.url.is_empty() || plugin_override.url == url)) {
        bool use = plugin_override.plugin.path == plugin->path;
        if (use)
          *plugin = plugin_override.plugin;
        return use;
      }
    }
  }

  // Check whether the plugin is disabled.
  ResourceContextMap::iterator prefs_it =
      resource_context_map_.find(context);
  if (prefs_it == resource_context_map_.end())
    return false;

  PluginPrefs* plugin_prefs = prefs_it->second.get();
  if (!plugin_prefs->IsPluginEnabled(*plugin))
    return false;

  // Check whether the plugin is restricted to a URL.
  RestrictedPluginMap::const_iterator it =
      restricted_plugins_.find(plugin->path);
  if (it != restricted_plugins_.end()) {
    if (it->second.first != plugin_prefs)
      return false;
    const GURL& origin = it->second.second;
    if (!origin.is_empty() &&
        (policy_url.scheme() != origin.scheme() ||
         policy_url.host() != origin.host() ||
         policy_url.port() != origin.port())) {
      return false;
    }
  }

  return true;
}

void ChromePluginServiceFilter::NPAPIPluginLoaded(
    int render_process_id,
    int render_frame_id,
    const std::string& mime_type,
    const content::WebPluginInfo& plugin) {
  PluginFinder* finder = PluginFinder::GetInstance();
  scoped_ptr<PluginMetadata> metadata(finder->GetPluginMetadata(plugin));

  // Singleton will outlive message loop so safe to use base::Unretained here.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&ChromePluginServiceFilter::ShowNPAPIInfoBar,
                 base::Unretained(this), render_process_id, render_frame_id,
                 metadata->name(), mime_type, false));
}

#if defined(OS_WIN) || defined(OS_MACOSX)
void ChromePluginServiceFilter::NPAPIPluginNotFound(
    int render_process_id,
    int render_frame_id,
    const std::string& mime_type) {
  // Singleton will outlive message loop so safe to use base::Unretained here.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&ChromePluginServiceFilter::ShowNPAPIInfoBar,
                 base::Unretained(this), render_process_id, render_frame_id,
                 base::string16(), mime_type, true));
}
#endif

bool ChromePluginServiceFilter::CanLoadPlugin(int render_process_id,
                                              const base::FilePath& path) {
  // The browser itself sometimes loads plugins to e.g. clear plugin data.
  // We always grant the browser permission.
  if (!render_process_id)
    return true;

  base::AutoLock auto_lock(lock_);
  const ProcessDetails* details = GetProcess(render_process_id);
  if (!details)
    return false;

  return (ContainsKey(details->authorized_plugins, path) ||
          ContainsKey(details->authorized_plugins, base::FilePath()));
}

void ChromePluginServiceFilter::ShowNPAPIInfoBar(int render_process_id,
                                                 int render_frame_id,
                                                 const base::string16& name,
                                                 const std::string& mime_type,
                                                 bool is_removed) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto ret = infobared_plugin_mime_types_.insert(mime_type);

  // Only display infobar once per mime type.
  if (!ret.second)
    return;

  base::string16 plugin_name(name);

  if (plugin_name.empty()) {
    plugin_name =
        PluginFinder::GetInstance()->FindPluginName(mime_type, "en-US");
  }

  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);

  content::WebContents* tab =
      content::WebContents::FromRenderFrameHost(render_frame_host);

  // WebContents could have been destroyed between posting and running the task
  // on the UI thread, so explicit check here.
  if (!tab)
    return;

  InfoBarService* infobar_service = InfoBarService::FromWebContents(tab);

  // NPAPI plugins can load inside extensions and if so there is nowhere to
  // display the infobar.
  if (infobar_service)
    NPAPIRemovalInfoBarDelegate::Create(infobar_service, plugin_name,
                                        is_removed);
}

void ChromePluginServiceFilter::AuthorizePlugin(
    int render_process_id,
    const base::FilePath& plugin_path) {
  base::AutoLock auto_lock(lock_);
  ProcessDetails* details = GetOrRegisterProcess(render_process_id);
  details->authorized_plugins.insert(plugin_path);
}

void ChromePluginServiceFilter::AuthorizeAllPlugins(
    content::WebContents* web_contents,
    bool load_blocked,
    const std::string& identifier) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  web_contents->ForEachFrame(base::Bind(&AuthorizeRenderer));
  if (load_blocked) {
    web_contents->SendToAllFrames(new ChromeViewMsg_LoadBlockedPlugins(
        MSG_ROUTING_NONE, identifier));
  }
}

ChromePluginServiceFilter::ChromePluginServiceFilter() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_PLUGIN_ENABLE_STATUS_CHANGED,
                 content::NotificationService::AllSources());
}

ChromePluginServiceFilter::~ChromePluginServiceFilter() {
}

void ChromePluginServiceFilter::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  switch (type) {
    case content::NOTIFICATION_RENDERER_PROCESS_CLOSED: {
      int render_process_id =
          content::Source<content::RenderProcessHost>(source).ptr()->GetID();

      base::AutoLock auto_lock(lock_);
      plugin_details_.erase(render_process_id);
      break;
    }
    case chrome::NOTIFICATION_PLUGIN_ENABLE_STATUS_CHANGED: {
      Profile* profile = content::Source<Profile>(source).ptr();
      PluginService::GetInstance()->PurgePluginListCache(profile, false);
      if (profile && profile->HasOffTheRecordProfile()) {
        PluginService::GetInstance()->PurgePluginListCache(
            profile->GetOffTheRecordProfile(), false);
      }
      break;
    }
    default: {
      NOTREACHED();
    }
  }
}

ChromePluginServiceFilter::ProcessDetails*
ChromePluginServiceFilter::GetOrRegisterProcess(
    int render_process_id) {
  return &plugin_details_[render_process_id];
}

const ChromePluginServiceFilter::ProcessDetails*
ChromePluginServiceFilter::GetProcess(
    int render_process_id) const {
  std::map<int, ProcessDetails>::const_iterator it =
      plugin_details_.find(render_process_id);
  if (it == plugin_details_.end())
    return NULL;
  return &it->second;
}

ChromePluginServiceFilter::OverriddenPlugin::OverriddenPlugin()
    : render_frame_id(MSG_ROUTING_NONE) {
}

ChromePluginServiceFilter::OverriddenPlugin::~OverriddenPlugin() {
}

ChromePluginServiceFilter::ProcessDetails::ProcessDetails() {
}

ChromePluginServiceFilter::ProcessDetails::~ProcessDetails() {
}

