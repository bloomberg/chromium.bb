// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_BROWSER_IN_PROCESS_WEBKIT_DOM_STORAGE_CONTEXT_H_
#define CHROME_BROWSER_IN_PROCESS_WEBKIT_DOM_STORAGE_CONTEXT_H_

#include <map>
#include <set>

#include "base/file_path.h"
#include "base/time.h"

class DOMStorageArea;
class DOMStorageDispatcherHost;
class DOMStorageNamespace;
class WebKitContext;

// This is owned by WebKitContext and is all the dom storage information that's
// shared by all the ResourceMessageFilter/DOMStorageDispatcherHosts that share
// the same profile.  The specifics of responsibilities are fairly well
// documented here and in StorageNamespace and StorageArea.  Everything is only
// to be accessed on the WebKit thread unless noted otherwise.
//
// NOTE: Virtual methods facilitate mocking functions for testing.
class DOMStorageContext {
 public:
  explicit DOMStorageContext(WebKitContext* webkit_context);
  virtual ~DOMStorageContext();

  // Get the local storage instance.  The pointer is owned by this class.
  DOMStorageNamespace* LocalStorage();

  // Get a new session storage namespace (but it's still owned by this class).
  DOMStorageNamespace* NewSessionStorage();

  // Allocate a new storage area id.  Only call on the WebKit thread.
  int64 AllocateStorageAreaId();

  // Allocate a new session storage id.  Only call on the UI or IO thread.
  int64 AllocateSessionStorageNamespaceId();

  // Clones a session storage namespace and returns the cloned namespaces' id.
  // Only call on the IO thread.
  int64 CloneSessionStorage(int64 original_id);

  // Various storage area methods.  The storage area is owned by one of the
  // namespaces that's owned by this class.
  void RegisterStorageArea(DOMStorageArea* storage_area);
  void UnregisterStorageArea(DOMStorageArea* storage_area);
  DOMStorageArea* GetStorageArea(int64 id);

  // Get a namespace from an id.  What's returned is owned by this class.  The
  // caller of GetStorageNamespace must immediately register itself with the
  // returned StorageNamespace.
  void RegisterStorageNamespace(DOMStorageNamespace* storage_namespace);
  void UnregisterStorageNamespace(DOMStorageNamespace* storage_namespace);
  DOMStorageNamespace* GetStorageNamespace(int64 id);

  // Sometimes an event from one DOM storage dispatcher host requires
  // communication to all of them.
  typedef std::set<DOMStorageDispatcherHost*> DispatcherHostSet;
  void RegisterDispatcherHost(DOMStorageDispatcherHost* dispatcher_host);
  void UnregisterDispatcherHost(DOMStorageDispatcherHost* dispatcher_host);
  const DispatcherHostSet* GetDispatcherHostSet() const;

  // Tells storage namespaces to purge any memory they do not need.
  virtual void PurgeMemory();

  // Delete any local storage files that have been touched since the cutoff
  // date that's supplied.
  void DeleteDataModifiedSince(const base::Time& cutoff);

 private:
  // The WebKit thread half of CloneSessionStorage above.  Static because
  // DOMStorageContext isn't ref counted thus we can't use a runnable method.
  // That said, we know this is safe because this class is destroyed on the
  // WebKit thread, so there's no way it could be destroyed before this is run.
  static void CompleteCloningSessionStorage(DOMStorageContext* context,
                                            int64 existing_id, int64 clone_id);

  // The last used storage_area_id and storage_namespace_id's.  For the storage
  // namespaces, IDs allocated on the UI thread are positive and count up while
  // IDs allocated on the IO thread are negative and count down.  This allows us
  // to allocate unique IDs on both without any locking.  All storage area ids
  // are allocated on the WebKit thread.
  int64 last_storage_area_id_;
  int64 last_session_storage_namespace_id_on_ui_thread_;
  int64 last_session_storage_namespace_id_on_io_thread_;

  // We're owned by this WebKit context.  Used while instantiating LocalStorage.
  WebKitContext* webkit_context_;

  // All the DOMStorageDispatcherHosts that are attached to us. ONLY USE ON THE
  // IO THREAD!
  DispatcherHostSet dispatcher_host_set_;

  // Maps ids to StorageAreas.  We do NOT own these objects.  StorageNamespace
  // (which does own them) will notify us when we should remove the entries.
  typedef std::map<int64, DOMStorageArea*> StorageAreaMap;
  StorageAreaMap storage_area_map_;

  // Maps ids to StorageNamespaces.  We own these objects.
  typedef std::map<int64, DOMStorageNamespace*> StorageNamespaceMap;
  StorageNamespaceMap storage_namespace_map_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(DOMStorageContext);
};

#endif  // CHROME_BROWSER_IN_PROCESS_WEBKIT_DOM_STORAGE_CONTEXT_H_
