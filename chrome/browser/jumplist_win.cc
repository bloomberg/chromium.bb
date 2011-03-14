// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/jumplist_win.h"

#include <windows.h>
#include <shobjidl.h>
#include <propkey.h>
#include <propvarutil.h>

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/scoped_comptr_win.h"
#include "base/string_util.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "base/win/windows_version.h"
#include "chrome/browser/favicon_service.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/page_usage_data.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browser_thread.h"
#include "googleurl/src/gurl.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/icon_util.h"


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

// Represents a class which encapsulates a PROPVARIANT object containing a
// string for AddShellLink().
// This class automatically deletes all the resources attached to the
// PROPVARIANT object in its destructor.
class PropVariantString {
 public:
  PropVariantString() {
    property_.vt = VT_EMPTY;
  }

  HRESULT Init(const std::wstring& value) {
    // Call InitPropVariantFromString() to initialize this PROPVARIANT object.
    // To read <propvarutil.h>, it seems InitPropVariantFromString() is an
    // inline function that initialize a PROPVARIANT object and calls
    // SHStrDupW() to set a copy of its input string.
    // So, we just calls it without creating a copy.
    return InitPropVariantFromString(value.c_str(), &property_);
  }

  ~PropVariantString() {
    if (property_.vt != VT_EMPTY)
      PropVariantClear(&property_);
  }

  const PROPVARIANT& Get() {
    return property_;
  }

 private:
  PROPVARIANT property_;

  DISALLOW_COPY_AND_ASSIGN(PropVariantString);
};

// Creates an IShellLink object.
// An IShellLink object is almost the same as an application shortcut, and it
// requires three items: the absolute path to an application, an argument
// string, and a title string.
HRESULT AddShellLink(ScopedComPtr<IObjectCollection> collection,
                     const std::wstring& application,
                     const std::wstring& switches,
                     scoped_refptr<ShellLinkItem> item) {
  // Create an IShellLink object.
  ScopedComPtr<IShellLink> link;
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
  ScopedComPtr<IPropertyStore> property_store;
  result = link.QueryInterface(property_store.Receive());
  if (FAILED(result))
    return result;

  PropVariantString property_title;
  result = property_title.Init(item->title());
  if (FAILED(result))
    return result;

  result = property_store->SetValue(PKEY_Title, property_title.Get());
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
                    const FilePath& icon_dir,
                    FilePath* icon_path) {
  // Retrieve the path to a temporary file.
  // We don't have to care about the extension of this temporary file because
  // JumpList does not care about it.
  FilePath path;
  if (!file_util::CreateTemporaryFileInDir(icon_dir, &path))
    return false;

  // Create an icon file from the favicon attached to the given |page|, and
  // save it as the temporary file.
  if (!IconUtil::CreateIconFileFromSkBitmap(bitmap, path))
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
HRESULT UpdateCategory(ScopedComPtr<ICustomDestinationList> list,
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
  ScopedComPtr<IObjectCollection> collection;
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
  ScopedComPtr<IObjectArray> object_array;
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
HRESULT UpdateTaskCategory(ScopedComPtr<ICustomDestinationList> list,
                           const std::wstring& chrome_path,
                           const std::wstring& chrome_switches) {
  // Create an EnumerableObjectCollection object to be added items of the
  // "Task" category. (We can also use this object for the "Task" category.)
  ScopedComPtr<IObjectCollection> collection;
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
  ScopedComPtr<IObjectArray> object_array;
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
  ScopedComPtr<ICustomDestinationList> destination_list;
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
  ScopedComPtr<IObjectArray> removed;
  result = destination_list->BeginList(&max_slots, __uuidof(*removed),
                                       reinterpret_cast<void**>(&removed));
  if (FAILED(result))
    return false;

  // Retrieve the absolute path to "chrome.exe".
  FilePath chrome_path;
  if (!PathService::Get(base::FILE_EXE, &chrome_path))
    return false;

  // Retrieve the command-line switches of this process.
  CommandLine command_line(CommandLine::NO_PROGRAM);
  FilePath user_data_dir = CommandLine::ForCurrentProcess()->
      GetSwitchValuePath(switches::kUserDataDir);
  if (!user_data_dir.empty())
    command_line.AppendSwitchPath(switches::kUserDataDir, user_data_dir);

  std::wstring chrome_switches = command_line.command_line_string();

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

// Represents a task which updates an application JumpList.
// This task encapsulates all I/O tasks and OS-specific tasks required for
// updating a JumpList from Chromium, such as:
// * Deleting the directory containing temporary icon files;
// * Creating temporary icon files used by the JumpList;
// * Creating an ICustomDestinationList instance;
// * Adding items in the ICustomDestinationList instance.
// To spawn this task,
// 1. Prepare objects required by this task:
//    * a std::wstring that contains a temporary icons;
//    * a ShellLinkItemList that contains the items of the "Most Visited"
//      category, and;
//    * a ShellLinkItemList that contains the items of the "Recently Closed"
//      category.
// 2. Create a JumpListUpdateTask instance, and;
// 3. Post this task to the file thread.
class JumpListUpdateTask : public Task {
 public:
  JumpListUpdateTask(const wchar_t* app_id,
                     const FilePath& icon_dir,
                     const ShellLinkItemList& most_visited_pages,
                     const ShellLinkItemList& recently_closed_pages)
    : app_id_(app_id),
      icon_dir_(icon_dir),
      most_visited_pages_(most_visited_pages),
      recently_closed_pages_(recently_closed_pages) {
  }

 private:
  // Represents an entry point of this task.
  // When we post this task to a file thread, the thread calls this function.
  void Run();

  // App id to associate with the jump list.
  std::wstring app_id_;

  // The directory which contains JumpList icons.
  FilePath icon_dir_;

  // Items in the "Most Visited" category of the application JumpList.
  ShellLinkItemList most_visited_pages_;

  // Items in the "Recently Closed" category of the application JumpList.
  ShellLinkItemList recently_closed_pages_;
};

void JumpListUpdateTask::Run() {
  // Delete the directory which contains old icon files, rename the current
  // icon directory, and create a new directory which contains new JumpList
  // icon files.
  FilePath icon_dir_old(icon_dir_.value() + L"Old");
  if (file_util::PathExists(icon_dir_old))
    file_util::Delete(icon_dir_old, true);
  file_util::Move(icon_dir_, icon_dir_old);
  file_util::CreateDirectory(icon_dir_);

  // Create temporary icon files for shortcuts in the "Most Visited" category.
  for (ShellLinkItemList::const_iterator item = most_visited_pages_.begin();
       item != most_visited_pages_.end(); ++item) {
    SkBitmap icon_bitmap;
    if ((*item)->data().get() &&
        gfx::PNGCodec::Decode((*item)->data()->front(),
                              (*item)->data()->size(),
                              &icon_bitmap)) {
      FilePath icon_path;
      if (CreateIconFile(icon_bitmap, icon_dir_, &icon_path))
        (*item)->SetIcon(icon_path.value(), 0, true);
    }
  }

  // Create temporary icon files for shortcuts in the "Recently Closed"
  // category.
  for (ShellLinkItemList::const_iterator item = recently_closed_pages_.begin();
       item != recently_closed_pages_.end(); ++item) {
    SkBitmap icon_bitmap;
    if ((*item)->data().get() &&
        gfx::PNGCodec::Decode((*item)->data()->front(),
                              (*item)->data()->size(),
                              &icon_bitmap)) {
      FilePath icon_path;
      if (CreateIconFile(icon_bitmap, icon_dir_, &icon_path))
        (*item)->SetIcon(icon_path.value(), 0, true);
    }
  }

  // We finished collecting all resources needed for updating an appliation
  // JumpList. So, create a new JumpList and replace the current JumpList
  // with it.
  UpdateJumpList(app_id_.c_str(), most_visited_pages_, recently_closed_pages_);

  // Delete all items in these lists now since we don't need the ShellLinkItem
  // objects in these lists.
  most_visited_pages_.clear();
  recently_closed_pages_.clear();
}

}  // namespace

JumpList::JumpList() : profile_(NULL) {
}

JumpList::~JumpList() {
  RemoveObserver();
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

  TabRestoreService* tab_restore_service = profile->GetTabRestoreService();
  if (!tab_restore_service)
    return false;

  app_id_ = ShellIntegration::GetChromiumAppId(profile->GetPath());
  icon_dir_ = profile->GetPath().Append(chrome::kJumpListIconDirname);
  profile_ = profile;
  tab_restore_service->AddObserver(this);
  return true;
}

void JumpList::RemoveObserver() {
  if (profile_ && profile_->GetTabRestoreService())
    profile_->GetTabRestoreService()->RemoveObserver(this);
  profile_ = NULL;
}

void JumpList::TabRestoreServiceChanged(TabRestoreService* service) {
  // Added or removed a tab.
  // Exit if we are updating the application JumpList.
  if (!icon_urls_.empty())
    return;

  // Send a query to HistoryService and retrieve the "Most Visited" pages.
  // This code is copied from MostVisitedHandler::HandleGetMostVisited() to
  // emulate its behaviors.
  const int kMostVisitedScope = 90;
  const int kMostVisitedCount = 9;
  int result_count = kMostVisitedCount;
  HistoryService* history_service =
      profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  history_service->QuerySegmentUsageSince(
      &most_visited_consumer_,
      base::Time::Now() - base::TimeDelta::FromDays(kMostVisitedScope),
      result_count,
      NewCallback(this, &JumpList::OnSegmentUsageAvailable));
}

void JumpList::TabRestoreServiceDestroyed(TabRestoreService* service) {
}

bool JumpList::AddTab(const TabRestoreService::Tab* tab,
                      ShellLinkItemList* list,
                      size_t max_items) {
  // This code adds the URL and the title strings of the given tab to the
  // specified list.
  // This code is copied from RecentlyClosedTabsHandler::TabToValue().
  if (tab->navigations.empty() || list->size() >= max_items)
    return false;

  const TabNavigation& current_navigation =
      tab->navigations.at(tab->current_navigation_index);
  if (current_navigation.virtual_url() == GURL(chrome::kChromeUINewTabURL))
    return false;

  scoped_refptr<ShellLinkItem> link(new ShellLinkItem);
  std::string url = current_navigation.virtual_url().spec();
  link->SetArguments(UTF8ToWide(url));
  link->SetTitle(current_navigation.title());
  list->push_back(link);
  icon_urls_.push_back(make_pair(url, link));
  return true;
}

bool JumpList::AddWindow(const TabRestoreService::Window* window,
                         ShellLinkItemList* list,
                         size_t max_items) {
  // This code enumerates al the tabs in the given window object and add their
  // URLs and titles to the list.
  // This code is copied from RecentlyClosedTabsHandler::WindowToValue().
  if (window->tabs.empty()) {
    NOTREACHED();
    return false;
  }
  for (size_t i = 0; i < window->tabs.size(); ++i) {
    if (!AddTab(&window->tabs[i], list, max_items))
      return false;
  }

  return true;
}

bool JumpList::StartLoadingFavIcon() {
  if (icon_urls_.empty())
    return false;

  // Ask FaviconService if it has a favicon of a URL.
  // When FaviconService has one, it will call OnFavIconDataAvailable().
  GURL url(icon_urls_.front().first);
  FaviconService* favicon_service =
      profile_->GetFaviconService(Profile::EXPLICIT_ACCESS);
  FaviconService::Handle handle = favicon_service->GetFaviconForURL(
      url, &fav_icon_consumer_,
      NewCallback(this, &JumpList::OnFavIconDataAvailable));
  return true;
}

void JumpList::OnSegmentUsageAvailable(
    CancelableRequestProvider::Handle handle,
    std::vector<PageUsageData*>* data) {
  // Create a list of ShellLinkItem objects from the given list of
  // PageUsageData objects.
  // The command that opens a web page with chrome is:
  //   "chrome.exe <url-to-the-web-page>".
  // So, we create a ShellLinkItem object with the following parameters.
  // * arguments
  //   The URL of a PageUsagedata object (converted to std::wstring).
  // * title
  //   The title of a PageUsageData object. If this string is empty, we use
  //   the URL as our "Most Visited" page does.
  // * icon
  //   An empty string. This value is to be updated in OnFavIconDataAvailable().
  most_visited_pages_.clear();
  for (std::vector<PageUsageData*>::const_iterator page = data->begin();
       page != data->end(); ++page) {
    scoped_refptr<ShellLinkItem> link(new ShellLinkItem);
    std::string url = (*page)->GetURL().spec();
    link->SetArguments(UTF8ToWide(url));
    link->SetTitle(
        !(*page)->GetTitle().empty() ? (*page)->GetTitle() : link->arguments());
    most_visited_pages_.push_back(link);
    icon_urls_.push_back(make_pair(url, link));
  }

  // Create a list of ShellLinkItems from the "Recently Closed" pages.
  // As noted above, we create a ShellLinkItem objects with the following
  // parameters.
  // * arguments
  //   The last URL of the tab object.
  // * title
  //   The title of the last URL.
  // * icon
  //   An empty string. This value is to be updated in OnFavIconDataAvailable().
  // This code is copied from
  // RecentlyClosedTabsHandler::TabRestoreServiceChanged() to emulate it.
  const int kRecentlyClosedCount = 4;
  recently_closed_pages_.clear();
  TabRestoreService* tab_restore_service = profile_->GetTabRestoreService();
  const TabRestoreService::Entries& entries = tab_restore_service->entries();
  for (TabRestoreService::Entries::const_iterator it = entries.begin();
       it != entries.end(); ++it) {
    const TabRestoreService::Entry* entry = *it;
    if (entry->type == TabRestoreService::TAB) {
      AddTab(static_cast<const TabRestoreService::Tab*>(entry),
             &recently_closed_pages_, kRecentlyClosedCount);
    } else if (entry->type == TabRestoreService::WINDOW) {
      AddWindow(static_cast<const TabRestoreService::Window*>(entry),
                &recently_closed_pages_, kRecentlyClosedCount);
    }
  }

  // Send a query that retrieves the first favicon.
  StartLoadingFavIcon();
}

void JumpList::OnFavIconDataAvailable(
    FaviconService::Handle handle,
    bool know_favicon,
    scoped_refptr<RefCountedMemory> data,
    bool expired,
    GURL icon_url) {
  // Attach the received data to the ShellLinkItem object.
  // This data will be decoded by JumpListUpdateTask.
  if (know_favicon && data.get() && data->size()) {
    if (!icon_urls_.empty() && icon_urls_.front().second)
      icon_urls_.front().second->SetIconData(data);
  }

  // if we need to load more favicons, we send another query and exit.
  if (!icon_urls_.empty())
    icon_urls_.pop_front();
  if (StartLoadingFavIcon())
    return;

  // Finished loading all favicons needed by the application JumpList.
  // We create a JumpListUpdateTask that creates icon files, and we post it to
  // the file thread.
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      new JumpListUpdateTask(app_id_.c_str(), icon_dir_, most_visited_pages_,
                             recently_closed_pages_));

  // Delete all items in these lists since we don't need these lists any longer.
  most_visited_pages_.clear();
  recently_closed_pages_.clear();
}
