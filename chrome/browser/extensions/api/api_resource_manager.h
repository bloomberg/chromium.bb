// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_API_RESOURCE_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_API_API_RESOURCE_MANAGER_H_

#include <map>

#include "base/memory/linked_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"

using content::BrowserThread;

namespace extensions {

// An ApiResourceManager manages the lifetime of a set of resources that
// ApiFunctions use. Examples are sockets or USB connections.
template <class T>
class ApiResourceManager : public ProfileKeyedService,
                           public base::NonThreadSafe,
                           public content::NotificationObserver {
 public:
  explicit ApiResourceManager(const BrowserThread::ID thread_id)
      : thread_id_(thread_id),
        data_(new ApiResourceData(thread_id)) {
    registrar_.Add(this,
      chrome::NOTIFICATION_EXTENSION_UNLOADED,
      content::NotificationService::AllSources());
  }

  virtual ~ApiResourceManager() {
    DCHECK(CalledOnValidThread());
    DCHECK(BrowserThread::IsMessageLoopValid(thread_id_)) <<
        "A unit test is using an ApiResourceManager but didn't provide "
        "the thread message loop needed for that kind of resource. "
        "Please ensure that the appropriate message loop is operational.";

    BrowserThread::DeleteSoon(thread_id_, FROM_HERE, data_.release());
  }

  // Takes ownership.
  int Add(T* api_resource) {
    return data_->Add(api_resource);
  }

  void Remove(const std::string& extension_id, int api_resource_id) {
    data_->Remove(extension_id, api_resource_id);
  }

  T* Get(const std::string& extension_id, int api_resource_id) {
    return data_->Get(extension_id, api_resource_id);
  }

 protected:
  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    switch (type) {
      case chrome::NOTIFICATION_EXTENSION_UNLOADED: {
        std::string id =
            content::Details<extensions::UnloadedExtensionInfo>(details)->
                extension->id();
        data_->InitiateCleanup(id);
        break;
      }
    }
  }

 private:

  // ApiResourceData class handles resource bookkeeping on a thread
  // where resource lifetime is handled.
  class ApiResourceData {
   public:
    typedef std::map<int, linked_ptr<T> > ApiResourceMap;
    // Lookup map from extension id's to allocated resource id's.
    typedef std::map<std::string, base::hash_set<int> > ExtensionToResourceMap;

    explicit ApiResourceData(const BrowserThread::ID thread_id)
        : next_id_(1),
          thread_id_(thread_id) {
    }

    int Add(T* api_resource) {
      DCHECK(BrowserThread::CurrentlyOn(thread_id_));
      int id = GenerateId();
      if (id > 0) {
        linked_ptr<T> resource_ptr(api_resource);
        api_resource_map_[id] = resource_ptr;

        const std::string& extension_id = api_resource->owner_extension_id();
        if (extension_resource_map_.find(extension_id) ==
            extension_resource_map_.end()) {
          extension_resource_map_[extension_id] = base::hash_set<int>();
        }
        extension_resource_map_[extension_id].insert(id);

       return id;
     }
     return 0;
    }

    void Remove(const std::string& extension_id, int api_resource_id) {
      DCHECK(BrowserThread::CurrentlyOn(thread_id_));
      if (GetOwnedResource(extension_id, api_resource_id) != NULL) {
        DCHECK(extension_resource_map_.find(extension_id) !=
               extension_resource_map_.end());
        extension_resource_map_[extension_id].erase(api_resource_id);
        api_resource_map_.erase(api_resource_id);
      }
    }

    T* Get(const std::string& extension_id, int api_resource_id) {
      DCHECK(BrowserThread::CurrentlyOn(thread_id_));
      return GetOwnedResource(extension_id, api_resource_id);
    }

    void InitiateCleanup(const std::string& extension_id) {
      BrowserThread::PostTask(thread_id_, FROM_HERE,
          base::Bind(&ApiResourceData::CleanupResourcesFromExtension,
                     base::Unretained(this), extension_id));
    }

   private:
    T* GetOwnedResource(const std::string& extension_id,
                        int api_resource_id) {
      linked_ptr<T> ptr = api_resource_map_[api_resource_id];
      T* resource = ptr.get();
      if (resource && extension_id == resource->owner_extension_id())
        return resource;
      return NULL;
    }

    void CleanupResourcesFromExtension(const std::string& extension_id) {
      DCHECK(BrowserThread::CurrentlyOn(thread_id_));
      if (extension_resource_map_.find(extension_id) !=
          extension_resource_map_.end()) {
        base::hash_set<int>& resource_ids =
            extension_resource_map_[extension_id];
        for (base::hash_set<int>::iterator it = resource_ids.begin();
             it != resource_ids.end(); ++it) {
          api_resource_map_.erase(*it);
        }
        extension_resource_map_.erase(extension_id);
      }
    }

    int GenerateId() {
      return next_id_++;
    }

    int next_id_;
    const BrowserThread::ID thread_id_;
    ApiResourceMap api_resource_map_;
    ExtensionToResourceMap extension_resource_map_;
  };

  const BrowserThread::ID thread_id_;
  content::NotificationRegistrar registrar_;
  scoped_ptr<ApiResourceData> data_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_API_RESOURCE_MANAGER_H_
