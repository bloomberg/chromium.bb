// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_CHROME_URL_DATA_MANAGER_H_
#define CHROME_BROWSER_DOM_UI_CHROME_URL_DATA_MANAGER_H_
#pragma once

#include <map>
#include <string>

#include "base/singleton.h"
#include "base/task.h"
#include "base/ref_counted.h"
#include "chrome/browser/browser_thread.h"

class DictionaryValue;
class FilePath;
class GURL;
class MessageLoop;
class RefCountedMemory;
class URLRequestChromeJob;

namespace net {
class URLRequest;
class URLRequestJob;
}  // namespace net

// To serve dynamic data off of chrome: URLs, implement the
// ChromeURLDataManager::DataSource interface and register your handler
// with AddDataSource.

// ChromeURLDataManager lives on the IO thread, so any interfacing with
// it from the UI thread needs to go through an InvokeLater.
class ChromeURLDataManager {
 public:
  // Returns the singleton instance.
  static ChromeURLDataManager* GetInstance();

  typedef int RequestID;

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
      DataSource, BrowserThread::DeleteOnUIThread> {
   public:
    // See source_name_ and message_loop_ below for docs on these parameters.
    DataSource(const std::string& source_name,
               MessageLoop* message_loop);

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

    static void SetFontAndTextDirection(DictionaryValue* localized_strings);

   protected:
    friend class base::RefCountedThreadSafe<DataSource>;
    friend struct BrowserThread::DeleteOnThread<BrowserThread::UI>;
    friend class DeleteTask<DataSource>;

    virtual ~DataSource();

   private:
    // The name of this source.
    // E.g., for favicons, this could be "favicon", which results in paths for
    // specific resources like "favicon/34" getting sent to this source.
    const std::string source_name_;

    // The MessageLoop for the thread where this DataSource lives.
    // Used to send messages to the DataSource.
    MessageLoop* message_loop_;
  };

  // Add a DataSource to the collection of data sources.
  // Because we don't track users of a given path, we can't know when it's
  // safe to remove them, so the added source effectively leaks.
  // This could be improved in the future but currently the users of this
  // interface are conceptually permanent registration anyway.
  // Adding a second DataSource with the same name clobbers the first.
  // NOTE: Calling this from threads other the IO thread must be done via
  // InvokeLater.
  void AddDataSource(scoped_refptr<DataSource> source);
  // Called during shutdown, before destruction of |BrowserThread|.
  void RemoveDataSourceForTest(const char* source_name);  // For unit tests.
  void RemoveAllDataSources();  // For the browser.

  // Add/remove a path from the collection of file sources.
  // A file source acts like a file:// URL to the specified path.
  // Calling this from threads other the IO thread must be done via
  // InvokeLater.
  void AddFileSource(const std::string& source_name, const FilePath& path);
  void RemoveFileSource(const std::string& source_name);

  static net::URLRequestJob* Factory(net::URLRequest* request,
                                     const std::string& scheme);

 private:
  friend class URLRequestChromeJob;
  friend struct DefaultSingletonTraits<ChromeURLDataManager>;

  ChromeURLDataManager();
  ~ChromeURLDataManager();

  // Parse a URL into the components used to resolve its request.
  static void URLToRequest(const GURL& url,
                           std::string* source,
                           std::string* path);

  // Translate a chrome resource URL into a local file path if there is one.
  // Returns false if there is no file handler for this URL
  static bool URLToFilePath(const GURL& url, FilePath* file_path);

  // Called by the job when it's starting up.
  // Returns false if |url| is not a URL managed by this object.
  bool StartRequest(const GURL& url, URLRequestChromeJob* job);
  // Remove a request from the list of pending requests.
  void RemoveRequest(URLRequestChromeJob* job);

  // Returns true if the job exists in |pending_requests_|. False otherwise.
  // Called by ~URLRequestChromeJob to verify that |pending_requests_| is kept
  // up to date.
  bool HasPendingJob(URLRequestChromeJob* job) const;

  // Sent by Request::SendResponse.
  void DataAvailable(RequestID request_id,
                     scoped_refptr<RefCountedMemory> bytes);

  // File sources of data, keyed by source name (e.g. "inspector").
  typedef std::map<std::string, FilePath> FileSourceMap;
  FileSourceMap file_sources_;

  // Custom sources of data, keyed by source path (e.g. "favicon").
  typedef std::map<std::string, scoped_refptr<DataSource> > DataSourceMap;
  DataSourceMap data_sources_;

  // All pending URLRequestChromeJobs, keyed by ID of the request.
  // URLRequestChromeJob calls into this object when it's constructed and
  // destructed to ensure that the pointers in this map remain valid.
  typedef std::map<RequestID, URLRequestChromeJob*> PendingRequestMap;
  PendingRequestMap pending_requests_;

  // The ID we'll use for the next request we receive.
  RequestID next_request_id_;
};

// Since we have a single global ChromeURLDataManager, we don't need to
// grab a reference to it when creating Tasks involving it.
DISABLE_RUNNABLE_METHOD_REFCOUNT(ChromeURLDataManager);

// Register our special URL handler under our special URL scheme.
// Must be done once at startup.
void RegisterURLRequestChromeJob();

// Undoes the registration done by RegisterURLRequestChromeJob.
void UnregisterURLRequestChromeJob();

#endif  // CHROME_BROWSER_DOM_UI_CHROME_URL_DATA_MANAGER_H_
