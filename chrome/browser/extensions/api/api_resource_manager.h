// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_API_RESOURCE_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_API_API_RESOURCE_MANAGER_H_

#include <map>

#include "base/memory/linked_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace extensions {

// An ApiResourceManager manages the lifetime of a set of resources that
// ApiFunctions use. Examples are sockets or USB connections.
template <class T>
class ApiResourceManager : public ProfileKeyedService,
                           public base::NonThreadSafe {
 public:
  explicit ApiResourceManager(BrowserThread::ID thread_id)
      : next_id_(1),
        thread_id_(thread_id),
        api_resource_map_(new std::map<int, linked_ptr<T> >()) {
  }

  virtual ~ApiResourceManager() {
    DCHECK(CalledOnValidThread());

    DCHECK(BrowserThread::IsMessageLoopValid(thread_id_)) <<
        "A unit test is using an ApiResourceManager but didn't provide "
        "the thread message loop needed for that kind of resource. "
        "Please ensure that the appropriate message loop is operational.";

    BrowserThread::DeleteSoon(thread_id_, FROM_HERE, api_resource_map_);
  }

  // Takes ownership.
  int Add(T* api_resource) {
    DCHECK(BrowserThread::CurrentlyOn(thread_id_));
    int id = GenerateId();
    if (id > 0) {
      linked_ptr<T> resource_ptr(api_resource);
      (*api_resource_map_)[id] = resource_ptr;
      return id;
    }
    return 0;
  }

  void Remove(const std::string& extension_id, int api_resource_id) {
    DCHECK(BrowserThread::CurrentlyOn(thread_id_));
    if (GetOwnedResource(extension_id, api_resource_id) != NULL) {
      api_resource_map_->erase(api_resource_id);
    }
  }

  T* Get(const std::string& extension_id, int api_resource_id) {
    DCHECK(BrowserThread::CurrentlyOn(thread_id_));
    return GetOwnedResource(extension_id, api_resource_id);
  }

 private:
  int GenerateId() {
    return next_id_++;
  }

  T* GetOwnedResource(const std::string& extension_id,
                      int api_resource_id) {
    linked_ptr<T> ptr = (*api_resource_map_)[api_resource_id];
    T* resource = ptr.get();
    if (resource && extension_id == resource->owner_extension_id())
      return resource;
    return NULL;
  }

  int next_id_;
  BrowserThread::ID thread_id_;

  // We need finer-grained control over the lifetime of this instance than RAII
  // can give us.
  std::map<int, linked_ptr<T> >* api_resource_map_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_API_RESOURCE_MANAGER_H_
