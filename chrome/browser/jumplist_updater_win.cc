// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/jumplist_updater_win.h"

#include <windows.h>
#include <propkey.h>
#include <shobjidl.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "chrome/common/chrome_switches.h"

namespace {

// Creates an IShellLink object.
// An IShellLink object is almost the same as an application shortcut, and it
// requires three items: the absolute path to an application, an argument
// string, and a title string.
bool AddShellLink(base::win::ScopedComPtr<IObjectCollection> collection,
                  const std::wstring& application_path,
                  scoped_refptr<ShellLinkItem> item) {
  // Create an IShellLink object.
  base::win::ScopedComPtr<IShellLink> link;
  HRESULT result = link.CreateInstance(CLSID_ShellLink, NULL,
                                       CLSCTX_INPROC_SERVER);
  if (FAILED(result))
    return false;

  // Set the application path.
  // We should exit this function when this call fails because it doesn't make
  // any sense to add a shortcut that we cannot execute.
  result = link->SetPath(application_path.c_str());
  if (FAILED(result))
    return false;

  // Attach the command-line switches of this process before the given
  // arguments and set it as the arguments of this IShellLink object.
  // We also exit this function when this call fails because it isn't useful to
  // add a shortcut that cannot open the given page.
  std::wstring arguments(item->GetArguments());
  if (!arguments.empty()) {
    result = link->SetArguments(arguments.c_str());
    if (FAILED(result))
      return false;
  }

  // Attach the given icon path to this IShellLink object.
  // Since an icon is an optional item for an IShellLink object, so we don't
  // have to exit even when it fails.
  if (!item->icon_path().empty())
    link->SetIconLocation(item->icon_path().c_str(), item->icon_index());

  // Set the title of the IShellLink object.
  // The IShellLink interface does not have any functions which update its
  // title because this interface is originally for creating an application
  // shortcut which doesn't have titles.
  // So, we should use the IPropertyStore interface to set its title.
  base::win::ScopedComPtr<IPropertyStore> property_store;
  result = link.QueryInterface(property_store.Receive());
  if (FAILED(result))
    return false;

  if (!base::win::SetStringValueForPropertyStore(
          property_store.get(),
          PKEY_Title,
          item->title().c_str())) {
    return false;
  }

  // Add this IShellLink object to the given collection.
  return SUCCEEDED(collection->AddObject(link));
}

}  // namespace


// ShellLinkItem

ShellLinkItem::ShellLinkItem()
    : command_line_(CommandLine::NO_PROGRAM),
      icon_index_(0) {
}

ShellLinkItem::~ShellLinkItem() {}

std::wstring ShellLinkItem::GetArguments() const {
  return command_line_.GetArgumentsString();
}

CommandLine* ShellLinkItem::GetCommandLine() {
  return &command_line_;
}


// JumpListUpdater

JumpListUpdater::JumpListUpdater(const std::wstring& app_user_model_id)
    : app_user_model_id_(app_user_model_id),
      user_max_items_(0) {
}

JumpListUpdater::~JumpListUpdater() {
}

// static
bool JumpListUpdater::IsEnabled() {
  // JumpList is implemented only on Windows 7 or later.
  // Do not create custom JumpLists in tests. See http://crbug.com/389375.
  return base::win::GetVersion() >= base::win::VERSION_WIN7 &&
         !CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType);
}

bool JumpListUpdater::BeginUpdate() {
  // This instance is expected to be one-time-use only.
  DCHECK(!destination_list_.get());

  // Check preconditions.
  if (!JumpListUpdater::IsEnabled() || app_user_model_id_.empty())
    return false;

  // Create an ICustomDestinationList object and attach it to our application.
  HRESULT result = destination_list_.CreateInstance(CLSID_DestinationList, NULL,
                                                    CLSCTX_INPROC_SERVER);
  if (FAILED(result))
    return false;

  // Set the App ID for this JumpList.
  result = destination_list_->SetAppID(app_user_model_id_.c_str());
  if (FAILED(result))
    return false;

  // Start a transaction that updates the JumpList of this application.
  // This implementation just replaces the all items in this JumpList, so
  // we don't have to use the IObjectArray object returned from this call.
  // It seems Windows 7 RC (Build 7100) automatically checks the items in this
  // removed list and prevent us from adding the same item.
  UINT max_slots;
  base::win::ScopedComPtr<IObjectArray> removed;
  result = destination_list_->BeginList(&max_slots, __uuidof(*removed),
                                        removed.ReceiveVoid());
  if (FAILED(result))
    return false;

  user_max_items_ = max_slots;

  return true;
}

bool JumpListUpdater::CommitUpdate() {
  if (!destination_list_.get())
    return false;

  // Commit this transaction and send the updated JumpList to Windows.
  return SUCCEEDED(destination_list_->CommitList());
}

bool JumpListUpdater::AddTasks(const ShellLinkItemList& link_items) {
  if (!destination_list_.get())
    return false;

  // Retrieve the absolute path to "chrome.exe".
  base::FilePath application_path;
  if (!PathService::Get(base::FILE_EXE, &application_path))
    return false;

  // Create an EnumerableObjectCollection object to be added items of the
  // "Task" category.
  base::win::ScopedComPtr<IObjectCollection> collection;
  HRESULT result = collection.CreateInstance(CLSID_EnumerableObjectCollection,
                                             NULL, CLSCTX_INPROC_SERVER);
  if (FAILED(result))
    return false;

  // Add items to the "Task" category.
  for (ShellLinkItemList::const_iterator it = link_items.begin();
       it != link_items.end(); ++it) {
    AddShellLink(collection, application_path.value(), *it);
  }

  // We can now add the new list to the JumpList.
  // ICustomDestinationList::AddUserTasks() also uses the IObjectArray
  // interface to retrieve each item in the list. So, we retrieve the
  // IObjectArray interface from the EnumerableObjectCollection object.
  base::win::ScopedComPtr<IObjectArray> object_array;
  result = collection.QueryInterface(object_array.Receive());
  if (FAILED(result))
    return false;

  return SUCCEEDED(destination_list_->AddUserTasks(object_array));
}

bool JumpListUpdater::AddCustomCategory(const std::wstring& category_name,
                                        const ShellLinkItemList& link_items,
                                        size_t max_items) {
 if (!destination_list_.get())
    return false;

  // Retrieve the absolute path to "chrome.exe".
  base::FilePath application_path;
  if (!PathService::Get(base::FILE_EXE, &application_path))
    return false;

  // Exit this function when the given vector does not contain any items
  // because an ICustomDestinationList::AppendCategory() call fails in this
  // case.
  if (link_items.empty() || !max_items)
    return true;

  // Create an EnumerableObjectCollection object.
  // We once add the given items to this collection object and add this
  // collection to the JumpList.
  base::win::ScopedComPtr<IObjectCollection> collection;
  HRESULT result = collection.CreateInstance(CLSID_EnumerableObjectCollection,
                                             NULL, CLSCTX_INPROC_SERVER);
  if (FAILED(result))
    return false;

  for (ShellLinkItemList::const_iterator item = link_items.begin();
       item != link_items.end() && max_items > 0; ++item, --max_items) {
    scoped_refptr<ShellLinkItem> link(*item);
    AddShellLink(collection, application_path.value(), link);
  }

  // We can now add the new list to the JumpList.
  // The ICustomDestinationList::AppendCategory() function needs the
  // IObjectArray interface to retrieve each item in the list. So, we retrive
  // the IObjectArray interface from the IEnumerableObjectCollection object
  // and use it.
  // It seems the ICustomDestinationList::AppendCategory() function just
  // replaces all items in the given category with the ones in the new list.
  base::win::ScopedComPtr<IObjectArray> object_array;
  result = collection.QueryInterface(object_array.Receive());
  if (FAILED(result))
    return false;

  return SUCCEEDED(destination_list_->AppendCategory(category_name.c_str(),
                                                     object_array));
}
