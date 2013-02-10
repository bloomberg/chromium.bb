// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/jumplist_win.h"

#include <windows.h>
#include <shobjidl.h>
#include <propkey.h>
#include <propvarutil.h>

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_comptr.h"
#include "base/win/scoped_propvariant.h"
#include "base/win/windows_version.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/page_usage_data.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_source.h"
#include "googleurl/src/gurl.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/icon_util.h"

using content::BrowserThread;

namespace {

// COM interfaces used in this file.
// These interface declarations are copied from Windows SDK 7.0.
// TODO(hbono): Bug 16903: delete them when we use Windows SDK 7.0.
#ifndef __IObjectArray_INTERFACE_DEFINED__
#define __IObjectArray_INTERFACE_DEFINED__

MIDL_INTERFACE("92CA9DCD-5622-4bba-A805-5E9F541BD8C9")
IObjectArray : public IUnknown {
 public:
  virtual HRESULT STDMETHODCALLTYPE GetCount(
      /* [out] */ __RPC__out UINT *pcObjects) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetAt(
      /* [in] */ UINT uiIndex,
      /* [in] */ __RPC__in REFIID riid,
      /* [iid_is][out] */ __RPC__deref_out_opt void **ppv) = 0;
};

#endif  // __IObjectArray_INTERFACE_DEFINED__

#ifndef __IObjectCollection_INTERFACE_DEFINED__
#define __IObjectCollection_INTERFACE_DEFINED__

MIDL_INTERFACE("5632b1a4-e38a-400a-928a-d4cd63230295")
IObjectCollection : public IObjectArray {
 public:
  virtual HRESULT STDMETHODCALLTYPE AddObject(
      /* [in] */ __RPC__in_opt IUnknown *punk) = 0;
  virtual HRESULT STDMETHODCALLTYPE AddFromArray(
      /* [in] */ __RPC__in_opt IObjectArray *poaSource) = 0;
  virtual HRESULT STDMETHODCALLTYPE RemoveObjectAt(
      /* [in] */ UINT uiIndex) = 0;
  virtual HRESULT STDMETHODCALLTYPE Clear(void) = 0;
};

#endif  // __IObjectCollection_INTERFACE_DEFINED__

#ifndef __ICustomDestinationList_INTERFACE_DEFINED__
#define __ICustomDestinationList_INTERFACE_DEFINED__

typedef /* [v1_enum] */ enum tagKNOWNDESTCATEGORY {
  KDC_FREQUENT = 1,
  KDC_RECENT = (KDC_FREQUENT + 1)
} KNOWNDESTCATEGORY;

MIDL_INTERFACE("6332debf-87b5-4670-90c0-5e57b408a49e")
ICustomDestinationList : public IUnknown {
 public:
  virtual HRESULT STDMETHODCALLTYPE SetAppID(
      /* [string][in] */__RPC__in_string LPCWSTR pszAppID) = 0;
  virtual HRESULT STDMETHODCALLTYPE BeginList(
      /* [out] */ __RPC__out UINT *pcMaxSlots,
      /* [in] */ __RPC__in REFIID riid,
      /* [iid_is][out] */ __RPC__deref_out_opt void **ppv) = 0;
  virtual HRESULT STDMETHODCALLTYPE AppendCategory(
      /* [string][in] */ __RPC__in_string LPCWSTR pszCategory,
      /* [in] */ __RPC__in_opt IObjectArray *poa) = 0;
  virtual HRESULT STDMETHODCALLTYPE AppendKnownCategory(
      /* [in] */ KNOWNDESTCATEGORY category) = 0;
  virtual HRESULT STDMETHODCALLTYPE AddUserTasks(
      /* [in] */ __RPC__in_opt IObjectArray *poa) = 0;
  virtual HRESULT STDMETHODCALLTYPE CommitList(void) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetRemovedDestinations(
      /* [in] */ __RPC__in REFIID riid,
      /* [iid_is][out] */ __RPC__deref_out_opt void **ppv) = 0;
  virtual HRESULT STDMETHODCALLTYPE DeleteList(
      /* [string][in] */ __RPC__in_string LPCWSTR pszAppID) = 0;
  virtual HRESULT STDMETHODCALLTYPE AbortList(void) = 0;
};

#endif  // __ICustomDestinationList_INTERFACE_DEFINED__

// Class IDs used in this file.
// These class IDs must be defined in an anonymous namespace to avoid
// conflicts with ones defined in "shell32.lib" of Visual Studio 2008.
// TODO(hbono): Bug 16903: delete them when we use Windows SDK 7.0.
const CLSID CLSID_DestinationList = {
  0x77f10cf0, 0x3db5, 0x4966, {0xb5, 0x20, 0xb7, 0xc5, 0x4f, 0xd3, 0x5e, 0xd6}
};

const CLSID CLSID_EnumerableObjectCollection = {
  0x2d3468c1, 0x36a7, 0x43b6, {0xac, 0x24, 0xd3, 0xf0, 0x2f, 0xd9, 0x60, 0x7a}
};

};  // namespace

// END OF WINDOWS 7 SDK DEFINITIONS

namespace {

// Creates an IShellLink object.
// An IShellLink object is almost the same as an application shortcut, and it
// requires three items: the absolute path to an application, an argument
// string, and a title string.
HRESULT AddShellLink(base::win::ScopedComPtr<IObjectCollection> collection,
                     const std::wstring& application,
                     const std::wstring& switches,
                     scoped_refptr<ShellLinkItem> item) {
  // Create an IShellLink object.
  base::win::ScopedComPtr<IShellLink> link;
  HRESULT result = link.CreateInstance(CLSID_ShellLink, NULL,
                                       CLSCTX_INPROC_SERVER);
  if (FAILED(result))
    return result;

  // Set the application path.
  // We should exit this function when this call fails because it doesn't make
  // any sense to add a shortcut that we cannot execute.
  result = link->SetPath(application.c_str());
  if (FAILED(result))
    return result;

  // Attach the command-line switches of this process before the given
  // arguments and set it as the arguments of this IShellLink object.
  // We also exit this function when this call fails because it isn't usuful to
  // add a shortcut that cannot open the given page.
  std::wstring arguments(switches);
  if (!item->arguments().empty()) {
    arguments.push_back(L' ');
    arguments += item->arguments();
  }
  result = link->SetArguments(arguments.c_str());
  if (FAILED(result))
    return result;

  // Attach the given icon path to this IShellLink object.
  // Since an icon is an optional item for an IShellLink object, so we don't
  // have to exit even when it fails.
  if (!item->icon().empty())
    link->SetIconLocation(item->icon().c_str(), item->index());

  // Set the title of the IShellLink object.
  // The IShellLink interface does not have any functions which update its
  // title because this interface is originally for creating an application
  // shortcut which doesn't have titles.
  // So, we should use the IPropertyStore interface to set its title as
  // listed in the steps below:
  // 1. Retrieve the IPropertyStore interface from the IShellLink object;
  // 2. Start a transaction that changes a value of the object with the
  //    IPropertyStore interface;
  // 3. Create a string PROPVARIANT, and;
  // 4. Call the IPropertyStore::SetValue() function to Set the title property
  //    of the IShellLink object.
  // 5. Commit the transaction.
  base::win::ScopedComPtr<IPropertyStore> property_store;
  result = link.QueryInterface(property_store.Receive());
  if (FAILED(result))
    return result;

  base::win::ScopedPropVariant property_title;
  // Call InitPropVariantFromString() to initialize |property_title|. Reading
  // <propvarutil.h>, it seems InitPropVariantFromString() is an inline function
  // that initializes a PROPVARIANT object and calls SHStrDupW() to set a copy
  // of its input string. It is thus safe to call it without first creating a
  // copy here.
  result = InitPropVariantFromString(item->title().c_str(),
                                     property_title.Receive());
  if (FAILED(result))
    return result;

  result = property_store->SetValue(PKEY_Title, property_title.get());
  if (FAILED(result))
    return result;

  result = property_store->Commit();
  if (FAILED(result))
    return result;

  // Add this IShellLink object to the given collection.
  return collection->AddObject(link);
}

// Creates a temporary icon file to be shown in JumpList.
bool CreateIconFile(const SkBitmap& bitmap,
                    const base::FilePath& icon_dir,
                    base::FilePath* icon_path) {
  // Retrieve the path to a temporary file.
  // We don't have to care about the extension of this temporary file because
  // JumpList does not care about it.
  base::FilePath path;
  if (!file_util::CreateTemporaryFileInDir(icon_dir, &path))
    return false;

  // Create an icon file from the favicon attached to the given |page|, and
  // save it as the temporary file.
  if (!IconUtil::CreateIconFileFromSkBitmap(bitmap, SkBitmap(), path))
    return false;

  // Add this icon file to the list and return its absolute path.
  // The IShellLink::SetIcon() function needs the absolute path to an icon.
  *icon_path = path;
  return true;
}

// Updates a specified category of an application JumpList.
// This function cannot update registered categories (such as "Tasks") because
// special steps are required for updating them.
// So, this function can be used only for adding an unregistered category.
// Parameters:
// * category_id (int)
//   A string ID which contains the category name.
// * application (std::wstring)
//   An application name to be used for creating JumpList items.
//   Even though we can add command-line switches to this parameter, it is
//   better to use the |switches| parameter below.
// * switches (std::wstring)
//   Command-lien switches for the application. This string is to be added
//   before the arguments of each ShellLinkItem object. If there aren't any
//   switches, use an empty string.
// * data (ShellLinkItemList)
//   A list of ShellLinkItem objects to be added under the specified category.
HRESULT UpdateCategory(base::win::ScopedComPtr<ICustomDestinationList> list,
                       int category_id,
                       const std::wstring& application,
                       const std::wstring& switches,
                       const ShellLinkItemList& data,
                       int max_slots) {
  // Exit this function when the given vector does not contain any items
  // because an ICustomDestinationList::AppendCategory() call fails in this
  // case.
  if (data.empty() || !max_slots)
    return S_OK;

  std::wstring category = UTF16ToWide(l10n_util::GetStringUTF16(category_id));

  // Create an EnumerableObjectCollection object.
  // We once add the given items to this collection object and add this
  // collection to the JumpList.
  base::win::ScopedComPtr<IObjectCollection> collection;
  HRESULT result = collection.CreateInstance(CLSID_EnumerableObjectCollection,
                                             NULL, CLSCTX_INPROC_SERVER);
  if (FAILED(result))
    return false;

  for (ShellLinkItemList::const_iterator item = data.begin();
       item != data.end() && max_slots > 0; ++item, --max_slots) {
    scoped_refptr<ShellLinkItem> link(*item);
    AddShellLink(collection, application, switches, link);
  }

  // We can now add the new list to the JumpList.
  // The ICustomDestinationList::AppendCategory() function needs the
  // IObjectArray interface to retrieve each item in the list. So, we retrive
  // the IObjectArray interface from the IEnumeratableObjectCollection object
  // and use it.
  // It seems the ICustomDestinationList::AppendCategory() function just
  // replaces all items in the given category with the ones in the new list.
  base::win::ScopedComPtr<IObjectArray> object_array;
  result = collection.QueryInterface(object_array.Receive());
  if (FAILED(result))
    return false;

  return list->AppendCategory(category.c_str(), object_array);
}

// Updates the "Tasks" category of the JumpList.
// Even though this function is almost the same as UpdateCategory(), this
// function has the following differences:
// * The "Task" category is a registered category.
//   We should use AddUserTasks() instead of AppendCategory().
// * The items in the "Task" category are static.
//   We don't have to use a list.
HRESULT UpdateTaskCategory(base::win::ScopedComPtr<ICustomDestinationList> list,
                           const std::wstring& chrome_path,
                           const std::wstring& chrome_switches) {
  // Create an EnumerableObjectCollection object to be added items of the
  // "Task" category. (We can also use this object for the "Task" category.)
  base::win::ScopedComPtr<IObjectCollection> collection;
  HRESULT result = collection.CreateInstance(CLSID_EnumerableObjectCollection,
                                             NULL, CLSCTX_INPROC_SERVER);
  if (FAILED(result))
    return result;

  // Create an IShellLink object which launches Chrome, and add it to the
  // collection. We use our application icon as the icon for this item.
  // We remove '&' characters from this string so we can share it with our
  // system menu.
  scoped_refptr<ShellLinkItem> chrome(new ShellLinkItem);
  std::wstring chrome_title =
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_NEW_WINDOW));
  ReplaceSubstringsAfterOffset(&chrome_title, 0, L"&", L"");
  chrome->SetTitle(chrome_title);
  chrome->SetIcon(chrome_path, 0, false);
  AddShellLink(collection, chrome_path, chrome_switches, chrome);

  // Create an IShellLink object which launches Chrome in incognito mode, and
  // add it to the collection. We use our application icon as the icon for
  // this item.
  scoped_refptr<ShellLinkItem> incognito(new ShellLinkItem);
  incognito->SetArguments(
      ASCIIToWide(std::string("--") + switches::kIncognito));
  std::wstring incognito_title =
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_NEW_INCOGNITO_WINDOW));
  ReplaceSubstringsAfterOffset(&incognito_title, 0, L"&", L"");
  incognito->SetTitle(incognito_title);
  incognito->SetIcon(chrome_path, 0, false);
  AddShellLink(collection, chrome_path, chrome_switches, incognito);

  // We can now add the new list to the JumpList.
  // ICustomDestinationList::AddUserTasks() also uses the IObjectArray
  // interface to retrieve each item in the list. So, we retrieve the
  // IObjectArray interface from the EnumerableObjectCollection object.
  base::win::ScopedComPtr<IObjectArray> object_array;
  result = collection.QueryInterface(object_array.Receive());
  if (FAILED(result))
    return result;

  return list->AddUserTasks(object_array);
}

// Updates the application JumpList.
// This function encapsulates all OS-specific operations required for updating
// the Chromium JumpList, such as:
// * Creating an ICustomDestinationList instance;
// * Updating the categories of the ICustomDestinationList instance, and;
// * Sending it to Taskbar of Windows 7.
bool UpdateJumpList(const wchar_t* app_id,
                    const ShellLinkItemList& most_visited_pages,
                    const ShellLinkItemList& recently_closed_pages) {
  // JumpList is implemented only on Windows 7 or later.
  // So, we should return now when this function is called on earlier versions
  // of Windows.
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return true;

  // Create an ICustomDestinationList object and attach it to our application.
  base::win::ScopedComPtr<ICustomDestinationList> destination_list;
  HRESULT result = destination_list.CreateInstance(CLSID_DestinationList, NULL,
                                                   CLSCTX_INPROC_SERVER);
  if (FAILED(result))
    return false;

  // Set the App ID for this JumpList.
  destination_list->SetAppID(app_id);

  // Start a transaction that updates the JumpList of this application.
  // This implementation just replaces the all items in this JumpList, so
  // we don't have to use the IObjectArray object returned from this call.
  // It seems Windows 7 RC (Build 7100) automatically checks the items in this
  // removed list and prevent us from adding the same item.
  UINT max_slots;
  base::win::ScopedComPtr<IObjectArray> removed;
  result = destination_list->BeginList(&max_slots, __uuidof(*removed),
                                       reinterpret_cast<void**>(&removed));
  if (FAILED(result))
    return false;

  // Retrieve the absolute path to "chrome.exe".
  base::FilePath chrome_path;
  if (!PathService::Get(base::FILE_EXE, &chrome_path))
    return false;

  // Retrieve the command-line switches of this process.
  CommandLine command_line(CommandLine::NO_PROGRAM);
  base::FilePath user_data_dir = CommandLine::ForCurrentProcess()->
      GetSwitchValuePath(switches::kUserDataDir);
  if (!user_data_dir.empty())
    command_line.AppendSwitchPath(switches::kUserDataDir, user_data_dir);

  std::wstring chrome_switches = command_line.GetCommandLineString();

  // We allocate 60% of the given JumpList slots to "most-visited" items
  // and 40% to "recently-closed" items, respectively.
  // Nevertheless, if there are not so many items in |recently_closed_pages|,
  // we give the remaining slots to "most-visited" items.
  const int kMostVisited = 60;
  const int kRecentlyClosed = 40;
  const int kTotal = kMostVisited + kRecentlyClosed;
  size_t most_visited_items = MulDiv(max_slots, kMostVisited, kTotal);
  size_t recently_closed_items = max_slots - most_visited_items;
  if (recently_closed_pages.size() < recently_closed_items) {
    most_visited_items += recently_closed_items - recently_closed_pages.size();
    recently_closed_items = recently_closed_pages.size();
  }

  // Update the "Most Visited" category of the JumpList.
  // This update request is applied into the JumpList when we commit this
  // transaction.
  result = UpdateCategory(destination_list, IDS_NEW_TAB_MOST_VISITED,
                          chrome_path.value(), chrome_switches,
                          most_visited_pages, most_visited_items);
  if (FAILED(result))
    return false;

  // Update the "Recently Closed" category of the JumpList.
  result = UpdateCategory(destination_list, IDS_NEW_TAB_RECENTLY_CLOSED,
                          chrome_path.value(), chrome_switches,
                          recently_closed_pages, recently_closed_items);
  if (FAILED(result))
    return false;

  // Update the "Tasks" category of the JumpList.
  result = UpdateTaskCategory(destination_list, chrome_path.value(),
                              chrome_switches);
  if (FAILED(result))
    return false;

  // Commit this transaction and send the updated JumpList to Windows.
  result = destination_list->CommitList();
  if (FAILED(result))
    return false;

  return true;
}

}  // namespace

JumpList::JumpList()
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      profile_(NULL),
      task_id_(CancelableTaskTracker::kBadTaskId) {
}

JumpList::~JumpList() {
  Terminate();
}

// static
bool JumpList::Enabled() {
  return (base::win::GetVersion() >= base::win::VERSION_WIN7 &&
          !CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kDisableCustomJumpList));
}

bool JumpList::AddObserver(Profile* profile) {
  // To update JumpList when a tab is added or removed, we add this object to
  // the observer list of the TabRestoreService class.
  // When we add this object to the observer list, we save the pointer to this
  // TabRestoreService object. This pointer is used when we remove this object
  // from the observer list.
  if (base::win::GetVersion() < base::win::VERSION_WIN7 || !profile)
    return false;

  TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(profile);
  if (!tab_restore_service)
    return false;

  app_id_ = ShellIntegration::GetChromiumModelIdForProfile(profile->GetPath());
  icon_dir_ = profile->GetPath().Append(chrome::kJumpListIconDirname);
  profile_ = profile;
  history::TopSites* top_sites = profile_->GetTopSites();
  if (top_sites) {
    // TopSites updates itself after a delay. This is especially noticable when
    // your profile is empty. Ask TopSites to update itself when jumplist is
    // initialized.
    top_sites->SyncWithHistory();
    registrar_.reset(new content::NotificationRegistrar);
    // Register for notification when TopSites changes so that we can update
    // ourself.
    registrar_->Add(this, chrome::NOTIFICATION_TOP_SITES_CHANGED,
                    content::Source<history::TopSites>(top_sites));
    // Register for notification when profile is destroyed to ensure that all
    // observers are detatched at that time.
    registrar_->Add(this, chrome::NOTIFICATION_PROFILE_DESTROYED,
                    content::Source<Profile>(profile_));
  }
  tab_restore_service->AddObserver(this);
  return true;
}

void JumpList::Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_TOP_SITES_CHANGED: {
      // Most visited urls changed, query again.
      history::TopSites* top_sites = profile_->GetTopSites();
      if (top_sites) {
        top_sites->GetMostVisitedURLs(
            base::Bind(&JumpList::OnMostVisitedURLsAvailable,
                       weak_ptr_factory_.GetWeakPtr()));
      }
      break;
    }
    case chrome::NOTIFICATION_PROFILE_DESTROYED: {
      // Profile was destroyed, do clean-up.
      Terminate();
      break;
    }
    default:
      NOTREACHED() << "Unexpected notification type.";
  }
}

void JumpList::RemoveObserver() {
  if (profile_) {
    TabRestoreService* tab_restore_service =
        TabRestoreServiceFactory::GetForProfile(profile_);
    if (tab_restore_service)
      tab_restore_service->RemoveObserver(this);
    registrar_.reset();
  }
  profile_ = NULL;
}

void JumpList::CancelPendingUpdate() {
  if (task_id_ != CancelableTaskTracker::kBadTaskId) {
    cancelable_task_tracker_.TryCancel(task_id_);
    task_id_ = CancelableTaskTracker::kBadTaskId;
  }
}

void JumpList::Terminate() {
  CancelPendingUpdate();
  RemoveObserver();
}

void JumpList::OnMostVisitedURLsAvailable(
    const history::MostVisitedURLList& data) {

  // If we have a pending favicon request, cancel it here (it is out of date).
  CancelPendingUpdate();

  {
    base::AutoLock auto_lock(list_lock_);
    most_visited_pages_.clear();
    for (size_t i = 0; i < data.size(); i++) {
      const history::MostVisitedURL& url = data[i];
      scoped_refptr<ShellLinkItem> link(new ShellLinkItem);
      std::string url_string = url.url.spec();
      link->SetArguments(UTF8ToWide(url_string));
      link->SetTitle(!url.title.empty()? url.title : link->arguments());
      most_visited_pages_.push_back(link);
      icon_urls_.push_back(make_pair(url_string, link));
    }
  }

  // Send a query that retrieves the first favicon.
  StartLoadingFavicon();
}

void JumpList::TabRestoreServiceChanged(TabRestoreService* service) {
  // if we have a pending handle request, cancel it here (it is out of date).
  CancelPendingUpdate();

  // local list to pass to methods
  ShellLinkItemList temp_list;

  // Create a list of ShellLinkItems from the "Recently Closed" pages.
  // As noted above, we create a ShellLinkItem objects with the following
  // parameters.
  // * arguments
  //   The last URL of the tab object.
  // * title
  //   The title of the last URL.
  // * icon
  //   An empty string. This value is to be updated in OnFaviconDataAvailable().
  // This code is copied from
  // RecentlyClosedTabsHandler::TabRestoreServiceChanged() to emulate it.
  const int kRecentlyClosedCount = 4;
  TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(profile_);
  const TabRestoreService::Entries& entries = tab_restore_service->entries();
  for (TabRestoreService::Entries::const_iterator it = entries.begin();
       it != entries.end(); ++it) {
    const TabRestoreService::Entry* entry = *it;
    if (entry->type == TabRestoreService::TAB) {
      AddTab(static_cast<const TabRestoreService::Tab*>(entry),
             &temp_list, kRecentlyClosedCount);
    } else if (entry->type == TabRestoreService::WINDOW) {
      AddWindow(static_cast<const TabRestoreService::Window*>(entry),
                &temp_list, kRecentlyClosedCount);
    }
  }
  // Lock recently_closed_pages and copy temp_list into it.
  {
    base::AutoLock auto_lock(list_lock_);
    recently_closed_pages_ = temp_list;
  }

  // Send a query that retrieves the first favicon.
  StartLoadingFavicon();
}

void JumpList::TabRestoreServiceDestroyed(TabRestoreService* service) {
}

bool JumpList::AddTab(const TabRestoreService::Tab* tab,
                      ShellLinkItemList* list,
                      size_t max_items) {
  // This code adds the URL and the title strings of the given tab to the
  // specified list.
  if (list->size() >= max_items)
    return false;

  scoped_refptr<ShellLinkItem> link(new ShellLinkItem);
  const TabNavigation& current_navigation =
      tab->navigations.at(tab->current_navigation_index);
  std::string url = current_navigation.virtual_url().spec();
  link->SetArguments(UTF8ToWide(url));
  link->SetTitle(current_navigation.title());
  list->push_back(link);
  icon_urls_.push_back(make_pair(url, link));
  return true;
}

void JumpList::AddWindow(const TabRestoreService::Window* window,
                         ShellLinkItemList* list,
                         size_t max_items) {
  // This code enumerates al the tabs in the given window object and add their
  // URLs and titles to the list.
  DCHECK(!window->tabs.empty());

  for (size_t i = 0; i < window->tabs.size(); ++i) {
    if (!AddTab(&window->tabs[i], list, max_items))
      return;
  }
}

void JumpList::StartLoadingFavicon() {
  GURL url;
  {
    base::AutoLock auto_lock(list_lock_);
    if (icon_urls_.empty()) {
      // No more favicons are needed by the application JumpList. Schedule a
      // RunUpdate call.
      BrowserThread::PostTask(
          BrowserThread::FILE, FROM_HERE,
          base::Bind(&JumpList::RunUpdate, this));
      return;
    }
    // Ask FaviconService if it has a favicon of a URL.
    // When FaviconService has one, it will call OnFaviconDataAvailable().
    url = GURL(icon_urls_.front().first);
  }
  FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile_, Profile::EXPLICIT_ACCESS);
  task_id_ = favicon_service->GetFaviconImageForURL(
      FaviconService::FaviconForURLParams(profile_,
                                          url,
                                          history::FAVICON,
                                          gfx::kFaviconSize),
      base::Bind(&JumpList::OnFaviconDataAvailable,
                 base::Unretained(this)),
      &cancelable_task_tracker_);
}

void JumpList::OnFaviconDataAvailable(
    const history::FaviconImageResult& image_result) {
  // If there is currently a favicon request in progress, it is now outdated,
  // as we have received another, so nullify the handle from the old request.
  task_id_ = CancelableTaskTracker::kBadTaskId;
  // lock the list to set icon data and pop the url
  {
    base::AutoLock auto_lock(list_lock_);
    // Attach the received data to the ShellLinkItem object.
    // This data will be decoded by the RunUpdate method.
    if (!image_result.image.IsEmpty()) {
      if (!icon_urls_.empty() && icon_urls_.front().second)
        icon_urls_.front().second->SetIconData(image_result.image.AsBitmap());
    }

    if (!icon_urls_.empty())
      icon_urls_.pop_front();
  }
  // Check whether we need to load more favicons.
  StartLoadingFavicon();
}

void JumpList::RunUpdate() {
  ShellLinkItemList local_most_visited_pages;
  ShellLinkItemList local_recently_closed_pages;

  {
    base::AutoLock auto_lock(list_lock_);
    // Make sure we are not out of date: if icon_urls_ is not empty, then
    // another notification has been received since we processed this one
    if (!icon_urls_.empty())
      return;

    // Make local copies of lists so we can release the lock.
    local_most_visited_pages = most_visited_pages_;
    local_recently_closed_pages = recently_closed_pages_;
  }

  // Delete the directory which contains old icon files, rename the current
  // icon directory, and create a new directory which contains new JumpList
  // icon files.
  base::FilePath icon_dir_old(icon_dir_.value() + L"Old");
  if (file_util::PathExists(icon_dir_old))
    file_util::Delete(icon_dir_old, true);
  file_util::Move(icon_dir_, icon_dir_old);
  file_util::CreateDirectory(icon_dir_);

  // Create temporary icon files for shortcuts in the "Most Visited" category.
  CreateIconFiles(local_most_visited_pages);

  // Create temporary icon files for shortcuts in the "Recently Closed"
  // category.
  CreateIconFiles(local_recently_closed_pages);

  // We finished collecting all resources needed for updating an appliation
  // JumpList. So, create a new JumpList and replace the current JumpList
  // with it.
  UpdateJumpList(app_id_.c_str(), local_most_visited_pages,
                 local_recently_closed_pages);
}

void JumpList::CreateIconFiles(const ShellLinkItemList& item_list) {
  for (ShellLinkItemList::const_iterator item = item_list.begin();
      item != item_list.end(); ++item) {
    base::FilePath icon_path;
    if (CreateIconFile((*item)->data(), icon_dir_, &icon_path))
      (*item)->SetIcon(icon_path.value(), 0, true);
  }
}
