// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_ARCHIVER_H_
#define COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_ARCHIVER_H_

#include "base/files/file_path.h"
#include "url/gurl.h"

namespace offline_pages {

// Interface of a class responsible for creation of the archive for offline use.
//
// Archiver will be implemented by embedder and may have additional methods that
// are not interesting from the perspective of OfflinePageModel. Example of such
// extra information or capability is a way to enumerate available WebContents
// to find the one that needs to be used to create archive (or to map it to the
// URL passed in CreateArchive in some other way).
//
// Archiver will be responsible for naming the file that is being saved (it has
// URL, title and the whole page content at its disposal). For that it should be
// also configured with the path where the archives are stored.
//
// Archiver should be able to archive multiple pages in parallel, as these are
// asynchronous calls carried out by some other component.
//
// If archiver gets two consecutive requests to archive the same page (may be
// run in parallel) it can generate 2 different names for files and save the
// same page separately, as if these were 2 completely unrelated pages. It is up
// to the caller (e.g. OfflinePageModel) to make sure that situation like that
// does not happen.
//
// If the page is not completely loaded, it is up to the implementation of the
// archiver whether to respond with ERROR_CONTENT_UNAVAILBLE, wait longer to
// actually snapshot a complete page, or snapshot whatever is available at that
// point in time (what the user sees).
//
// TODO(fgorski): Add ability to delete archive.
// TODO(fgorski): Add ability to check that archive exists.
// TODO(fgorski): Add ability to refresh an existing archive in one step.
// TODO(fgorski): Add ability to identify all of the archives in the directory,
// to enable to model to reconcile the archives.
class OfflinePageArchiver {
 public:
  // Represents an in progress request to archive a page.
  class Request {
   public:
    virtual ~Request() {}

    // Cancels an in progress request to archive a page.
    virtual void Cancel() = 0;
    virtual const GURL& url() const = 0;
  };

  // Errors that will be reported when archive creation fails.
  enum ArchiverResult {
    SUCCESSFULLY_CREATED,       // Archive created successfully.
    ERROR_UNKNOWN,              // Don't know what went wrong.
    ERROR_DEVICE_FULL,          // Cannot save the archive - device is full.
    ERROR_CANCELLED,            // Caller cancelled the request.
    ERROR_CONTENT_UNAVAILABLE,  // Content to archive is not available.
  };

  // Interface of the clients that requests to archive pages.
  class Client {
   public:
    virtual ~Client() {}

    // Callback called by the archiver when archiver creation is complete.
    // |result| will indicate SUCCESSFULLY_CREATED upon success, or a specific
    // error, when it failed.
    virtual void OnCreateArchiveDone(Request* request,
                                     ArchiverResult result,
                                     const base::FilePath& file_path) = 0;
  };

  virtual ~OfflinePageArchiver() {}

  // Starts creating the archive. Will pass result by calling methods on the
  // passed in client. Caller owns the returned request object.
  // If request is deleted during the archiving, the callback will not be
  // invoked. The archive might however be created.
  virtual scoped_ptr<Request> CreateArchive(const GURL& url,
                                            Client* client) = 0;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_ARCHIVER_H_
