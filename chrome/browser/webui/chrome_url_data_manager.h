// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBUI_CHROME_URL_DATA_MANAGER_H_
#define CHROME_BROWSER_WEBUI_CHROME_URL_DATA_MANAGER_H_
#pragma once

#include <string>

#include "base/ref_counted.h"
#include "base/task.h"
#include "chrome/browser/browser_thread.h"

class ChromeURLDataManagerBackend;
class DictionaryValue;
class FilePath;
class MessageLoop;
class Profile;
class RefCountedMemory;

// To serve dynamic data off of chrome: URLs, implement the
// ChromeURLDataManager::DataSource interface and register your handler
// with AddDataSource. DataSources must be added on the UI thread (they are also
// deleted on the UI thread). Internally the DataSources are maintained by
// ChromeURLDataManagerBackend, see it for details.
class ChromeURLDataManager {
 public:
  class DataSource;

  // Trait used to handle deleting a DataSource. Deletion happens on the UI
  // thread.
  //
  // Implementation note: the normal shutdown sequence is for the UI loop to
  // stop pumping events then the IO loop and thread are stopped. When the
  // DataSources are no longer referenced (which happens when IO thread stops)
  // they get added to the UI message loop for deletion. But because the UI loop
  // has stopped by the time this happens the DataSources would be leaked.
  //
  // To make sure DataSources are properly deleted ChromeURLDataManager manages
  // deletion of the DataSources.  When a DataSource is no longer referenced it
  // is added to |data_sources_| and a task is posted to the UI thread to handle
  // the actual deletion. During shutdown |DeleteDataSources| is invoked so that
  // all pending DataSources are properly deleted.
  struct DeleteDataSource {
    static void Destruct(const DataSource* data_source) {
      ChromeURLDataManager::DeleteDataSource(data_source);
    }
  };

  // A DataSource is an object that can answer requests for data
  // asynchronously. DataSources are collectively owned with refcounting smart
  // pointers and should never be deleted on the IO thread, since their calls
  // are handled almost always on the UI thread and there's a possibility of a
  // data race.  The |DeleteOnUIThread| trait is used to enforce this.
  //
  // An implementation of DataSource should handle calls to
  // StartDataRequest() by starting its (implementation-specific) asynchronous
  // request for the data, then call SendResponse() to notify.
  class DataSource : public base::RefCountedThreadSafe<
      DataSource, DeleteDataSource> {
   public:
    // See source_name_ and message_loop_ below for docs on these parameters.
    DataSource(const std::string& source_name, MessageLoop* message_loop);

    // Sent by the DataManager to request data at |path|.  The source should
    // call SendResponse() when the data is available or if the request could
    // not be satisfied.
    virtual void StartDataRequest(const std::string& path,
                                  bool is_off_the_record,
                                  int request_id) = 0;

    // Return the mimetype that should be sent with this response, or empty
    // string to specify no mime type.
    virtual std::string GetMimeType(const std::string& path) const = 0;

    // Report that a request has resulted in the data |bytes|.
    // If the request can't be satisfied, pass NULL for |bytes| to indicate
    // the request is over.
    virtual void SendResponse(int request_id, RefCountedMemory* bytes);

    // Returns the MessageLoop on which the DataSource wishes to have
    // StartDataRequest called to handle the request for |path|.  If the
    // DataSource does not care which thread StartDataRequest is called on,
    // this should return NULL.  The default implementation always returns
    // message_loop_, which generally results in processing on the UI thread.
    // It may be beneficial to return NULL for requests that are safe to handle
    // directly on the IO thread.  This can improve performance by satisfying
    // such requests more rapidly when there is a large amount of UI thread
    // contention.
    virtual MessageLoop* MessageLoopForRequestPath(const std::string& path)
        const;

    const std::string& source_name() const { return source_name_; }

    // Returns true if this DataSource should replace an existing DataSource
    // with the same name that has already been registered. The default is
    // true.
    //
    // WARNING: this is invoked on the IO thread.
    //
    // TODO: nuke this and convert all callers to not replace.
    virtual bool ShouldReplaceExistingSource() const;

    static void SetFontAndTextDirection(DictionaryValue* localized_strings);

   protected:
    virtual ~DataSource();

   private:
    friend class ChromeURLDataManagerBackend;
    friend class ChromeURLDataManager;
    friend class DeleteTask<DataSource>;

    // SendResponse invokes this on the IO thread. Notifies the backend to
    // handle the actual work of sending the data.
    virtual void SendResponseOnIOThread(int request_id,
                                        scoped_refptr<RefCountedMemory> bytes);

    // The name of this source.
    // E.g., for favicons, this could be "favicon", which results in paths for
    // specific resources like "favicon/34" getting sent to this source.
    const std::string source_name_;

    // The MessageLoop for the thread where this DataSource lives.
    // Used to send messages to the DataSource.
    MessageLoop* message_loop_;

    // This field is set and maintained by ChromeURLDataManagerBackend. It is
    // set when the DataSource is added, and unset if the DataSource is removed.
    // A DataSource can be removed in two ways: the ChromeURLDataManagerBackend
    // is deleted, or another DataSource is registered with the same
    // name. backend_ should only be accessed on the IO thread.
    // This reference can't be via a scoped_refptr else there would be a cycle
    // between the backend and data source.
    ChromeURLDataManagerBackend* backend_;
  };

  explicit ChromeURLDataManager(Profile* profile);
  ~ChromeURLDataManager();

  // Adds a DataSource to the collection of data sources. This *must* be invoked
  // on the UI thread.
  //
  // If |AddDataSource| is called more than once for a particular name it will
  // release the old |DataSource|, most likely resulting in it getting deleted
  // as there are no other references to it. |DataSource| uses the
  // |DeleteOnUIThread| trait to insure that the destructor is called on the UI
  // thread. This is necessary as some |DataSource|s notably |FileIconSource|
  // and |WebUIFavIconSource|, have members that will DCHECK if they are not
  // destructed in the same thread as they are constructed (the UI thread).
  void AddDataSource(DataSource* source);

  // Deletes any data sources no longer referenced. This is normally invoked
  // for you, but can be invoked to force deletion (such as during shutdown).
  static void DeleteDataSources();

 private:
  typedef std::vector<const ChromeURLDataManager::DataSource*> DataSources;

  // If invoked on the UI thread the DataSource is deleted immediatlye,
  // otherwise it is added to |data_sources_| and a task is scheduled to handle
  // deletion on the UI thread. See note abouve DeleteDataSource for more info.
  static void DeleteDataSource(const DataSource* data_source);

  // Returns true if |data_source| is scheduled for deletion (|DeleteDataSource|
  // was invoked).
  static bool IsScheduledForDeletion(const DataSource* data_source);

  Profile* profile_;

  // Lock used when accessing |data_sources_|.
  static base::Lock delete_lock_;

  // |data_sources_| that are no longer referenced and scheduled for deletion.
  static DataSources* data_sources_;

  DISALLOW_COPY_AND_ASSIGN(ChromeURLDataManager);
};

#endif  // CHROME_BROWSER_WEBUI_CHROME_URL_DATA_MANAGER_H_
