// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_util.h"

#include <string>

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/automation/automation_provider.h"
#include "chrome/browser/automation/automation_provider_json.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/app_modal_dialogs/app_modal_dialog_queue.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/browser_list_impl.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/view_type_utils.h"
#include "chrome/common/automation_id.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/login_display.h"
#include "chrome/browser/chromeos/login/login_display_host.h"
#include "chrome/browser/chromeos/login/webui_login_display.h"
#include "chrome/browser/chromeos/login/webui_login_display_host.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#endif

using content::BrowserThread;
using content::RenderViewHost;
using content::WebContents;

#if defined(OS_CHROMEOS)
using chromeos::ExistingUserController;
using chromeos::User;
using chromeos::UserManager;
#endif

namespace {

void GetCookiesCallback(base::WaitableEvent* event,
                        std::string* cookies,
                        const std::string& cookie_line) {
  *cookies = cookie_line;
  event->Signal();
}

void GetCookiesOnIOThread(
    const GURL& url,
    const scoped_refptr<net::URLRequestContextGetter>& context_getter,
    base::WaitableEvent* event,
    std::string* cookies) {
  context_getter->GetURLRequestContext()->cookie_store()->
      GetCookiesWithOptionsAsync(url, net::CookieOptions(),
                      base::Bind(&GetCookiesCallback, event, cookies));
}

void GetCanonicalCookiesCallback(
    base::WaitableEvent* event,
    net::CookieList* cookie_list,
    const net::CookieList& cookies) {
  *cookie_list = cookies;
  event->Signal();
}

void GetCanonicalCookiesOnIOThread(
    const GURL& url,
    const scoped_refptr<net::URLRequestContextGetter>& context_getter,
    base::WaitableEvent* event,
    net::CookieList* cookie_list) {
  context_getter->GetURLRequestContext()->cookie_store()->
      GetCookieMonster()->GetAllCookiesForURLAsync(
          url,
          base::Bind(&GetCanonicalCookiesCallback, event, cookie_list));
}

void SetCookieCallback(base::WaitableEvent* event,
                       bool* success,
                       bool result) {
  *success = result;
  event->Signal();
}

void SetCookieOnIOThread(
    const GURL& url,
    const std::string& value,
    const scoped_refptr<net::URLRequestContextGetter>& context_getter,
    base::WaitableEvent* event,
    bool* success) {
  context_getter->GetURLRequestContext()->cookie_store()->
      SetCookieWithOptionsAsync(
          url, value, net::CookieOptions(),
          base::Bind(&SetCookieCallback, event, success));
}

void SetCookieWithDetailsOnIOThread(
    const GURL& url,
    const net::CanonicalCookie& cookie,
    const std::string& original_domain,
    const scoped_refptr<net::URLRequestContextGetter>& context_getter,
    base::WaitableEvent* event,
    bool* success) {
  net::CookieMonster* cookie_monster =
      context_getter->GetURLRequestContext()->cookie_store()->
      GetCookieMonster();
  cookie_monster->SetCookieWithDetailsAsync(
      url, cookie.Name(), cookie.Value(), original_domain,
      cookie.Path(), cookie.ExpiryDate(), cookie.IsSecure(),
      cookie.IsHttpOnly(),
      base::Bind(&SetCookieCallback, event, success));
}

void DeleteCookieCallback(base::WaitableEvent* event) {
  event->Signal();
}

void DeleteCookieOnIOThread(
    const GURL& url,
    const std::string& name,
    const scoped_refptr<net::URLRequestContextGetter>& context_getter,
    base::WaitableEvent* event) {
  context_getter->GetURLRequestContext()->cookie_store()->
      DeleteCookieAsync(url, name,
                        base::Bind(&DeleteCookieCallback, event));
}

}  // namespace

namespace automation_util {

Browser* GetBrowserAt(int index) {
  // The automation layer doesn't support non-native desktops.
  chrome::BrowserListImpl* native_list =
      chrome::BrowserListImpl::GetInstance(chrome::HOST_DESKTOP_TYPE_NATIVE);
  if (index < 0 || index >= static_cast<int>(native_list->size()))
    return NULL;
  return native_list->get(index);
}

WebContents* GetWebContentsAt(int browser_index, int tab_index) {
  if (tab_index < 0)
    return NULL;
  Browser* browser = GetBrowserAt(browser_index);
  if (!browser || tab_index >= browser->tab_strip_model()->count())
    return NULL;
  return browser->tab_strip_model()->GetWebContentsAt(tab_index);
}

#if defined(OS_CHROMEOS)
Profile* GetCurrentProfileOnChromeOS(std::string* error_message) {
  const UserManager* user_manager = UserManager::Get();
  if (!user_manager) {
    *error_message = "No user manager.";
    return NULL;
  }
  if (!user_manager->IsUserLoggedIn()) {
    ExistingUserController* controller =
        ExistingUserController::current_controller();
    if (!controller) {
      *error_message = "Cannot get controller though user is not logged in.";
      return NULL;
    }
    chromeos::WebUILoginDisplayHost* webui_login_display_host =
        static_cast<chromeos::WebUILoginDisplayHost*>(
            controller->login_display_host());
    content::WebUI* web_ui = webui_login_display_host->GetOobeUI()->web_ui();
    if (!web_ui) {
      *error_message = "Unable to get webui from login display host.";
      return NULL;
    }
    return Profile::FromWebUI(web_ui);
  } else {
    ash::Shell* shell = ash::Shell::GetInstance();
    if (!shell || !shell->delegate()) {
      *error_message = "Unable to get shell delegate.";
      return NULL;
    }
    return Profile::FromBrowserContext(
        shell->delegate()->GetCurrentBrowserContext());
  }
  return NULL;
}
#endif  // defined(OS_CHROMEOS)

Browser* GetBrowserForTab(WebContents* tab) {
  for (chrome::BrowserIterator it; !it.done(); it.Next()) {
    Browser* browser = *it;
    for (int tab_index = 0;
         tab_index < browser->tab_strip_model()->count();
         ++tab_index) {
      if (browser->tab_strip_model()->GetWebContentsAt(tab_index) == tab)
        return browser;
    }
  }
  return NULL;
}

net::URLRequestContextGetter* GetRequestContext(WebContents* contents) {
  // Since we may be on the UI thread don't call GetURLRequestContext().
  // Get the request context specific to the current WebContents and app.
  return contents->GetBrowserContext()->GetRequestContextForRenderProcess(
      contents->GetRenderProcessHost()->GetID());
}

void GetCookies(const GURL& url,
                WebContents* contents,
                int* value_size,
                std::string* value) {
  *value_size = -1;
  if (url.is_valid() && contents) {
    scoped_refptr<net::URLRequestContextGetter> context_getter =
        GetRequestContext(contents);
    base::WaitableEvent event(true /* manual reset */,
                              false /* not initially signaled */);
    CHECK(BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&GetCookiesOnIOThread, url, context_getter, &event, value)));
    event.Wait();

    *value_size = static_cast<int>(value->size());
  }
}

void SetCookie(const GURL& url,
               const std::string& value,
               WebContents* contents,
               int* response_value) {
  *response_value = -1;

  if (url.is_valid() && contents) {
    scoped_refptr<net::URLRequestContextGetter> context_getter =
        GetRequestContext(contents);
    base::WaitableEvent event(true /* manual reset */,
                              false /* not initially signaled */);
    bool success = false;
    CHECK(BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&SetCookieOnIOThread, url, value, context_getter, &event,
                   &success)));
    event.Wait();
    if (success)
      *response_value = 1;
  }
}

void DeleteCookie(const GURL& url,
                  const std::string& cookie_name,
                  WebContents* contents,
                  bool* success) {
  *success = false;
  if (url.is_valid() && contents) {
    scoped_refptr<net::URLRequestContextGetter> context_getter =
        GetRequestContext(contents);
    base::WaitableEvent event(true /* manual reset */,
                              false /* not initially signaled */);
    CHECK(BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&DeleteCookieOnIOThread, url, cookie_name, context_getter,
                   &event)));
    event.Wait();
    *success = true;
  }
}

void GetCookiesJSON(AutomationProvider* provider,
                    DictionaryValue* args,
                    IPC::Message* reply_message) {
  AutomationJSONReply reply(provider, reply_message);
  std::string url;
  if (!args->GetString("url", &url)) {
    reply.SendError("'url' missing or invalid");
    return;
  }

  // Since we may be on the UI thread don't call GetURLRequestContext().
  scoped_refptr<net::URLRequestContextGetter> context_getter =
      provider->profile()->GetRequestContext();

  net::CookieList cookie_list;
  base::WaitableEvent event(true /* manual reset */,
                            false /* not initially signaled */);
  base::Closure task = base::Bind(&GetCanonicalCookiesOnIOThread, GURL(url),
                                  context_getter, &event, &cookie_list);
  if (!BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, task)) {
    reply.SendError("Couldn't post task to get the cookies");
    return;
  }
  event.Wait();

  ListValue* list = new ListValue();
  for (size_t i = 0; i < cookie_list.size(); ++i) {
    const net::CanonicalCookie& cookie = cookie_list[i];
    DictionaryValue* cookie_dict = new DictionaryValue();
    cookie_dict->SetString("name", cookie.Name());
    cookie_dict->SetString("value", cookie.Value());
    cookie_dict->SetString("path", cookie.Path());
    cookie_dict->SetString("domain", cookie.Domain());
    cookie_dict->SetBoolean("secure", cookie.IsSecure());
    cookie_dict->SetBoolean("http_only", cookie.IsHttpOnly());
    if (cookie.IsPersistent())
      cookie_dict->SetDouble("expiry", cookie.ExpiryDate().ToDoubleT());
    list->Append(cookie_dict);
  }
  DictionaryValue dict;
  dict.Set("cookies", list);
  reply.SendSuccess(&dict);
}

void DeleteCookieJSON(AutomationProvider* provider,
                      DictionaryValue* args,
                      IPC::Message* reply_message) {
  AutomationJSONReply reply(provider, reply_message);
  std::string url, name;
  if (!args->GetString("url", &url)) {
    reply.SendError("'url' missing or invalid");
    return;
  }
  if (!args->GetString("name", &name)) {
    reply.SendError("'name' missing or invalid");
    return;
  }

  // Since we may be on the UI thread don't call GetURLRequestContext().
  scoped_refptr<net::URLRequestContextGetter> context_getter =
      provider->profile()->GetRequestContext();

  base::WaitableEvent event(true /* manual reset */,
                            false /* not initially signaled */);
  base::Closure task = base::Bind(&DeleteCookieOnIOThread, GURL(url), name,
                                  context_getter, &event);
  if (!BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, task)) {
    reply.SendError("Couldn't post task to delete the cookie");
    return;
  }
  event.Wait();
  reply.SendSuccess(NULL);
}

void SetCookieJSON(AutomationProvider* provider,
                   DictionaryValue* args,
                   IPC::Message* reply_message) {
  AutomationJSONReply reply(provider, reply_message);
  std::string url;
  if (!args->GetString("url", &url)) {
    reply.SendError("'url' missing or invalid");
    return;
  }
  DictionaryValue* cookie_dict;
  if (!args->GetDictionary("cookie", &cookie_dict)) {
    reply.SendError("'cookie' missing or invalid");
    return;
  }
  std::string name, value;
  std::string domain;
  std::string path = "/";
  std::string mac_key;
  std::string mac_algorithm;
  bool secure = false;
  double expiry = 0;
  bool http_only = false;
  if (!cookie_dict->GetString("name", &name)) {
    reply.SendError("'name' missing or invalid");
    return;
  }
  if (!cookie_dict->GetString("value", &value)) {
    reply.SendError("'value' missing or invalid");
    return;
  }
  if (cookie_dict->HasKey("domain") &&
      !cookie_dict->GetString("domain", &domain)) {
    reply.SendError("optional 'domain' invalid");
    return;
  }
  if (cookie_dict->HasKey("path") &&
      !cookie_dict->GetString("path", &path)) {
    reply.SendError("optional 'path' invalid");
    return;
  }
  // mac_key and mac_algorithm are optional.
  if (cookie_dict->HasKey("mac_key"))
    cookie_dict->GetString("mac_key", &mac_key);
  if (cookie_dict->HasKey("mac_algorithm"))
    cookie_dict->GetString("mac_algorithm", &mac_algorithm);
  if (cookie_dict->HasKey("secure") &&
      !cookie_dict->GetBoolean("secure", &secure)) {
    reply.SendError("optional 'secure' invalid");
    return;
  }
  if (cookie_dict->HasKey("expiry")) {
    if (!cookie_dict->GetDouble("expiry", &expiry)) {
      reply.SendError("optional 'expiry' invalid");
      return;
    }
  }
  if (cookie_dict->HasKey("http_only") &&
      !cookie_dict->GetBoolean("http_only", &http_only)) {
    reply.SendError("optional 'http_only' invalid");
    return;
  }

  scoped_ptr<net::CanonicalCookie> cookie(
      net::CanonicalCookie::Create(
          GURL(url), name, value, domain, path,
          mac_key, mac_algorithm, base::Time(),
          base::Time::FromDoubleT(expiry), secure, http_only));
  if (!cookie.get()) {
    reply.SendError("given 'cookie' parameters are invalid");
    return;
  }

  // Since we may be on the UI thread don't call GetURLRequestContext().
  scoped_refptr<net::URLRequestContextGetter> context_getter =
      provider->profile()->GetRequestContext();

  base::WaitableEvent event(true /* manual reset */,
                            false /* not initially signaled */);
  bool success = false;
  base::Closure task = base::Bind(
      &SetCookieWithDetailsOnIOThread, GURL(url), *cookie.get(), domain,
      context_getter, &event, &success);
  if (!BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, task)) {
    reply.SendError("Couldn't post task to set the cookie");
    return;
  }
  event.Wait();

  if (!success) {
    reply.SendError("Could not set the cookie");
    return;
  }
  reply.SendSuccess(NULL);
}

bool SendErrorIfModalDialogActive(AutomationProvider* provider,
                                  IPC::Message* message) {
  bool active = AppModalDialogQueue::GetInstance()->HasActiveDialog();
  if (active) {
    AutomationJSONReply(provider, message).SendErrorCode(
        automation::kBlockedByModalDialog);
  }
  return active;
}

AutomationId GetIdForTab(const WebContents* tab) {
  const SessionTabHelper* session_tab_helper =
      SessionTabHelper::FromWebContents(tab);
  return AutomationId(AutomationId::kTypeTab,
                      base::IntToString(session_tab_helper->session_id().id()));
}

AutomationId GetIdForExtensionView(
    const content::RenderViewHost* render_view_host) {
  AutomationId::Type type;
  WebContents* web_contents = WebContents::FromRenderViewHost(render_view_host);
  switch (chrome::GetViewType(web_contents)) {
    case chrome::VIEW_TYPE_EXTENSION_POPUP:
      type = AutomationId::kTypeExtensionPopup;
      break;
    case chrome::VIEW_TYPE_EXTENSION_BACKGROUND_PAGE:
      type = AutomationId::kTypeExtensionBgPage;
      break;
    case chrome::VIEW_TYPE_EXTENSION_INFOBAR:
      type = AutomationId::kTypeExtensionInfobar;
      break;
    case chrome::VIEW_TYPE_APP_SHELL:
      type = AutomationId::kTypeAppShell;
      break;
    default:
      type = AutomationId::kTypeInvalid;
      break;
  }
  // Since these extension views do not permit navigation, using the
  // renderer process and view ID should suffice.
  std::string id = base::StringPrintf("%d|%d",
      render_view_host->GetRoutingID(),
      render_view_host->GetProcess()->GetID());
  return AutomationId(type, id);
}

AutomationId GetIdForExtension(const extensions::Extension* extension) {
  return AutomationId(AutomationId::kTypeExtension, extension->id());
}

bool GetTabForId(const AutomationId& id, WebContents** tab) {
  if (id.type() != AutomationId::kTypeTab)
    return false;

  printing::PrintPreviewDialogController* preview_controller =
      printing::PrintPreviewDialogController::GetInstance();
  for (chrome::BrowserIterator it; !it.done(); it.Next()) {
    Browser* browser = *it;
    for (int tab_index = 0;
         tab_index < browser->tab_strip_model()->count();
         ++tab_index) {
      WebContents* web_contents =
          browser->tab_strip_model()->GetWebContentsAt(tab_index);
      SessionTabHelper* session_tab_helper =
          SessionTabHelper::FromWebContents(web_contents);
      if (base::IntToString(
              session_tab_helper->session_id().id()) == id.id()) {
        *tab = web_contents;
        return true;
      }
      if (preview_controller) {
        WebContents* print_preview_contents =
            preview_controller->GetPrintPreviewForContents(web_contents);
        if (print_preview_contents) {
          SessionTabHelper* preview_session_tab_helper =
              SessionTabHelper::FromWebContents(print_preview_contents);
          std::string preview_id = base::IntToString(
              preview_session_tab_helper->session_id().id());
          if (preview_id == id.id()) {
            *tab = print_preview_contents;
            return true;
          }
        }
      }
    }
  }
  return false;
}

namespace {

bool GetExtensionRenderViewForId(
    const AutomationId& id,
    Profile* profile,
    RenderViewHost** rvh) {
  ExtensionProcessManager* extension_mgr =
      extensions::ExtensionSystem::Get(profile)->process_manager();
  const ExtensionProcessManager::ViewSet view_set =
      extension_mgr->GetAllViews();
  for (ExtensionProcessManager::ViewSet::const_iterator iter = view_set.begin();
       iter != view_set.end(); ++iter) {
    content::RenderViewHost* host = *iter;
    AutomationId this_id = GetIdForExtensionView(host);
    if (id == this_id) {
      *rvh = host;
      return true;
    }
  }
  return false;
}

}  // namespace

bool GetRenderViewForId(
    const AutomationId& id,
    Profile* profile,
    RenderViewHost** rvh) {
  switch (id.type()) {
    case AutomationId::kTypeTab: {
      WebContents* tab;
      if (!GetTabForId(id, &tab))
        return false;
      *rvh = tab->GetRenderViewHost();
      break;
    }
    case AutomationId::kTypeExtensionPopup:
    case AutomationId::kTypeExtensionBgPage:
    case AutomationId::kTypeExtensionInfobar:
    case AutomationId::kTypeAppShell:
      if (!GetExtensionRenderViewForId(id, profile, rvh))
        return false;
      break;
    default:
      return false;
  }
  return true;
}

bool GetExtensionForId(
    const AutomationId& id,
    Profile* profile,
    const extensions::Extension** extension) {
  if (id.type() != AutomationId::kTypeExtension)
    return false;
  ExtensionService* service = extensions::ExtensionSystem::Get(profile)->
      extension_service();
  const extensions::Extension* installed_extension =
      service->GetInstalledExtension(id.id());
  if (installed_extension)
    *extension = installed_extension;
  return !!installed_extension;
}

bool DoesObjectWithIdExist(const AutomationId& id, Profile* profile) {
  switch (id.type()) {
    case AutomationId::kTypeTab: {
      WebContents* tab;
      return GetTabForId(id, &tab);
    }
    case AutomationId::kTypeExtensionPopup:
    case AutomationId::kTypeExtensionBgPage:
    case AutomationId::kTypeExtensionInfobar:
    case AutomationId::kTypeAppShell: {
      RenderViewHost* rvh;
      return GetExtensionRenderViewForId(id, profile, &rvh);
    }
    case AutomationId::kTypeExtension: {
      const extensions::Extension* extension;
      return GetExtensionForId(id, profile, &extension);
    }
    default:
      break;
  }
  return false;
}

}  // namespace automation_util
