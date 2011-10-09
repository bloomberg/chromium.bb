// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_HTTP_SERVER_PROPERTIES_MANAGER_H_
#define CHROME_BROWSER_NET_HTTP_SERVER_PROPERTIES_MANAGER_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "content/common/notification_observer.h"
#include "net/base/host_port_pair.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_server_properties_impl.h"

class NotificationDetails;
class NotificationSource;
class PrefService;

namespace chrome_browser_net {

////////////////////////////////////////////////////////////////////////////////
//  HttpServerPropertiesManager

// The manager for creating and updating an HttpServerProperties (for example it
// tracks if a server supports SPDY or not).
//
// This class interacts with both the UI thread, where notifications of pref
// changes are received from, and the IO thread, which owns it (in the
// ProfileIOData) and it persists the changes from network stack that if a
// server supports SPDY or not.
//
// It must be constructed on the UI thread, to set up |ui_method_factory_| and
// the prefs listeners.
//
// ShutdownOnUIThread must be called from UI before destruction, to release
// the prefs listeners on the UI thread. This is done from ProfileIOData.
//
// Update tasks from the UI thread can post safely to the IO thread, since the
// destruction order of Profile and ProfileIOData guarantees that if this
// exists in UI, then a potential destruction on IO will come after any task
// posted to IO from that method on UI. This is used to go through IO before
// the actual update starts, and grab a WeakPtr.
class HttpServerPropertiesManager
    : public net::HttpServerProperties,
      public NotificationObserver {
 public:
  // Create an instance of the HttpServerPropertiesManager. The lifetime of the
  // PrefService objects must be longer than that of the
  // HttpServerPropertiesManager object. Must be constructed on the UI thread.
  explicit HttpServerPropertiesManager(PrefService* pref_service);
  virtual ~HttpServerPropertiesManager();

  // Initialize |http_server_properties_impl_| and |io_method_factory_| on IO
  // thread. It also posts a task to UI thread to get SPDY Server preferences
  // from |pref_service_|.
  void InitializeOnIOThread();

  // Prepare for shutdown. Must be called on the UI thread, before destruction.
  void ShutdownOnUIThread();

  // Returns true if |server| supports SPDY. Should only be called from IO
  // thread.
  virtual bool SupportsSpdy(const net::HostPortPair& server) const OVERRIDE;

  // Add |server| as the SPDY server which supports SPDY protocol into the
  // persisitent store. Should only be called from IO thread.
  virtual void SetSupportsSpdy(const net::HostPortPair& server,
                               bool support_spdy) OVERRIDE;

  // Deletes all data.
  virtual void DeleteAll() OVERRIDE;

  // Register |prefs| SPDY preferences.
  static void RegisterPrefs(PrefService* prefs);

 protected:
  typedef base::RefCountedData<base::ListValue> RefCountedListValue;

  // These are used to delay updating the spdy servers in
  // |http_server_properties_impl_| while the preferences are changing, and
  // execute only one update per simultaneous prefs changes.
  void ScheduleUpdateCacheOnUI();

  // Update spdy servers (the cached data in |http_server_properties_impl_|)
  // with data from preferences. Virtual for testing.
  virtual void UpdateCacheFromPrefs();

  // Starts the |spdy_servers| update on the IO thread. Protected for testing.
  void UpdateCacheFromPrefsOnIO(StringVector* spdy_servers, bool support_spdy);

  // These are used to delay updating the preferences when spdy servers_ are
  // changing, and execute only one update per simultaneous spdy server changes.
  void ScheduleUpdatePrefsOnIO();

  // Update spdy servers in preferences with the cached data from
  // |http_server_properties_impl_|. Virtual for testing.
  virtual void UpdatePrefsFromCache();  // Virtual for testing.

  // Update |prefs::kSpdyServers| preferences with |spdy_server_list| on UI
  // thread. Protected for testing.
  void SetSpdyServersInPrefsOnUI(
      scoped_refptr<RefCountedListValue> spdy_server_list);

 private:
  // Post the tasks with the delay. These are overridden in tests to post the
  // task without the delay.
  virtual void PostUpdateTaskOnUI(Task* task);  // Virtual for testing.
  virtual void PostUpdateTaskOnIO(Task* task);  // Virtual for testing.

  // Callback for preference changes.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // ---------
  // UI thread
  // ---------

  // Used to post update tasks to the UI thread.
  ScopedRunnableMethodFactory<HttpServerPropertiesManager> ui_method_factory_;

  // Used to get |weak_ptr_| to self on the UI thread.
  scoped_ptr<base::WeakPtrFactory<HttpServerPropertiesManager> >
      ui_weak_ptr_factory_;

  base::WeakPtr<HttpServerPropertiesManager> ui_weak_ptr_;

  // Used to track the spdy servers changes.
  PrefChangeRegistrar pref_change_registrar_;
  PrefService* pref_service_;  // Weak.

  // ---------
  // IO thread
  // ---------

  // Used to post update tasks to the IO thread.
  scoped_ptr<ScopedRunnableMethodFactory<HttpServerPropertiesManager> >
      io_method_factory_;

  scoped_ptr<net::HttpServerPropertiesImpl> http_server_properties_impl_;

  DISALLOW_COPY_AND_ASSIGN(HttpServerPropertiesManager);
};

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_HTTP_SERVER_PROPERTIES_MANAGER_H_
