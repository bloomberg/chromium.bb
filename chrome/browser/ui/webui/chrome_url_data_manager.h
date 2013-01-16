// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROME_URL_DATA_MANAGER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROME_URL_DATA_MANAGER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/profiles/profile_keyed_service.h"

class ChromeURLDataManagerBackend;
class MessageLoop;
class Profile;

namespace base {
class RefCountedMemory;
}

namespace content {
class URLDataSource;
}

class URLDataSourceImpl;

// To serve dynamic data off of chrome: URLs, implement the
// ChromeURLDataManager::DataSource interface and register your handler
// with AddDataSource. DataSources must be added on the UI thread (they are also
// deleted on the UI thread). Internally the DataSources are maintained by
// ChromeURLDataManagerBackend, see it for details.
class ChromeURLDataManager : public ProfileKeyedService {
 public:
  explicit ChromeURLDataManager(
      const base::Callback<ChromeURLDataManagerBackend*(void)>& backend);
  virtual ~ChromeURLDataManager();

  // Adds a DataSource to the collection of data sources. This *must* be invoked
  // on the UI thread.
  //
  // If |AddDataSource| is called more than once for a particular name it will
  // release the old |DataSource|, most likely resulting in it getting deleted
  // as there are no other references to it. |DataSource| uses the
  // |DeleteOnUIThread| trait to insure that the destructor is called on the UI
  // thread. This is necessary as some |DataSource|s notably |FileIconSource|
  // and |FaviconSource|, have members that will DCHECK if they are not
  // destructed in the same thread as they are constructed (the UI thread).
  void AddDataSource(URLDataSourceImpl* source);

  // Deletes any data sources no longer referenced. This is normally invoked
  // for you, but can be invoked to force deletion (such as during shutdown).
  static void DeleteDataSources();

  // Convenience wrapper function to add |source| to |profile|'s
  // |ChromeURLDataManager|. Creates a URLDataSourceImpl to wrap the given
  // source.
  static void AddDataSource(Profile* profile,
                            content::URLDataSource* source);

  // Like above, but takes a URLDataSourceImpl directly.
  static void AddDataSourceImpl(Profile* profile,
                                URLDataSourceImpl* source);

 private:
  friend class URLDataSourceImpl;
  friend struct DeleteURLDataSource;
  typedef std::vector<const URLDataSourceImpl*> URLDataSources;

  // If invoked on the UI thread the DataSource is deleted immediatlye,
  // otherwise it is added to |data_sources_| and a task is scheduled to handle
  // deletion on the UI thread. See note abouve DeleteDataSource for more info.
  static void DeleteDataSource(const URLDataSourceImpl* data_source);

  // Returns true if |data_source| is scheduled for deletion (|DeleteDataSource|
  // was invoked).
  static bool IsScheduledForDeletion(const URLDataSourceImpl* data_source);

  // A callback that returns the ChromeURLDataManagerBackend. Only accessible on
  // the IO thread. This is necessary because ChromeURLDataManager is created on
  // the UI thread, but ChromeURLDataManagerBackend lives on the IO thread.
  const base::Callback<ChromeURLDataManagerBackend*(void)> backend_;

  // |data_sources_| that are no longer referenced and scheduled for deletion.
  // Protected by g_delete_lock in the .cc file.
  static URLDataSources* data_sources_;

  DISALLOW_COPY_AND_ASSIGN(ChromeURLDataManager);
};

// Trait used to handle deleting a URLDataSource. Deletion happens on the UI
// thread.
//
// Implementation note: the normal shutdown sequence is for the UI loop to
// stop pumping events then the IO loop and thread are stopped. When the
// URLDataSources are no longer referenced (which happens when IO thread stops)
// they get added to the UI message loop for deletion. But because the UI loop
// has stopped by the time this happens the URLDataSources would be leaked.
//
// To make sure URLDataSources are properly deleted ChromeURLDataManager manages
// deletion of the URLDataSources.  When a URLDataSource is no longer referenced
// it is added to |data_sources_| and a task is posted to the UI thread to
// handle the actual deletion. During shutdown |DeleteDataSources| is invoked so
// that all pending URLDataSources are properly deleted.
struct DeleteURLDataSource {
  static void Destruct(const URLDataSourceImpl* data_source) {
    ChromeURLDataManager::DeleteDataSource(data_source);
  }
};

// A URLDataSource is an object that can answer requests for data
// asynchronously. URLDataSources are collectively owned with refcounting smart
// pointers and should never be deleted on the IO thread, since their calls
// are handled almost always on the UI thread and there's a possibility of a
// data race.  The |DeleteDataSource| trait above is used to enforce this.
class URLDataSourceImpl : public base::RefCountedThreadSafe<
    URLDataSourceImpl, DeleteURLDataSource> {
 public:
  // See source_name_ below for docs on that parameter. Takes ownership of
  // |source|.
  URLDataSourceImpl(const std::string& source_name,
                    content::URLDataSource* source);

  // Report that a request has resulted in the data |bytes|.
  // If the request can't be satisfied, pass NULL for |bytes| to indicate
  // the request is over.
  virtual void SendResponse(int request_id, base::RefCountedMemory* bytes);

  const std::string& source_name() const { return source_name_; }
  content::URLDataSource* source() const { return source_.get(); }

 protected:
  virtual ~URLDataSourceImpl();

 private:
  friend class ChromeURLDataManagerBackend;
  friend class ChromeURLDataManager;
  friend class base::DeleteHelper<URLDataSourceImpl>;

  // SendResponse invokes this on the IO thread. Notifies the backend to
  // handle the actual work of sending the data.
  virtual void SendResponseOnIOThread(
      int request_id,
      scoped_refptr<base::RefCountedMemory> bytes);

  // The name of this source.
  // E.g., for favicons, this could be "favicon", which results in paths for
  // specific resources like "favicon/34" getting sent to this source.
  const std::string source_name_;

  // This field is set and maintained by ChromeURLDataManagerBackend. It is
  // set when the DataSource is added, and unset if the DataSource is removed.
  // A DataSource can be removed in two ways: the ChromeURLDataManagerBackend
  // is deleted, or another DataSource is registered with the same
  // name. backend_ should only be accessed on the IO thread.
  // This reference can't be via a scoped_refptr else there would be a cycle
  // between the backend and data source.
  ChromeURLDataManagerBackend* backend_;

  scoped_ptr<content::URLDataSource> source_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_CHROME_URL_DATA_MANAGER_H_
