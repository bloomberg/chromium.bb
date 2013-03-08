// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Chromium settings and storage represent user-selected preferences and
// information and MUST not be extracted, overwritten or modified except
// through Chromium defined APIs.

#ifndef CHROME_BROWSER_WEBDATA_WEB_DATABASE_SERVICE_IMPL_H_
#define CHROME_BROWSER_WEBDATA_WEB_DATABASE_SERVICE_IMPL_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/webdata/web_database_service.h"

class WebDatabaseServiceInternal;
class WebDataRequestManager;

////////////////////////////////////////////////////////////////////////////////
//
// WebDatabaseService is responsible for controlling access to the web database
// (a generic data repository for metadata associated with web pages).
//
////////////////////////////////////////////////////////////////////////////////

class WebDatabaseServiceImpl : public ProfileKeyedService,
                               public WebDatabaseService {
 public:
  // Takes the path to the WebDatabase file.
  explicit WebDatabaseServiceImpl(const base::FilePath& path);

  virtual ~WebDatabaseServiceImpl();

  // WebDatabaseService overrides.
  virtual void LoadDatabase(const InitCallback& callback) OVERRIDE;
  virtual void UnloadDatabase() OVERRIDE;
  virtual WebDatabase* GetDatabase() const OVERRIDE;
  virtual void ScheduleDBTask(
      const tracked_objects::Location& from_here,
      const WriteTask& task) OVERRIDE;
  virtual WebDataServiceBase::Handle ScheduleDBTaskWithResult(
      const tracked_objects::Location& from_here,
      const ReadTask& task,
      WebDataServiceConsumer* consumer) OVERRIDE;
  virtual void CancelRequest(WebDataServiceBase::Handle h) OVERRIDE;

 protected:
  // To be used for testing purposes only.
  void set_init_complete(bool complete);

 private:
  base::FilePath path_;
  scoped_refptr<WebDatabaseServiceInternal> wdbs_internal_;

  DISALLOW_COPY_AND_ASSIGN(WebDatabaseServiceImpl);
};

#endif  // CHROME_BROWSER_WEBDATA_WEB_DATABASE_SERVICE_IMPL_H_
