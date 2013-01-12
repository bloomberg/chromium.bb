// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Chromium settings and storage represent user-selected preferences and
// information and MUST not be extracted, overwritten or modified except
// through Chromium defined APIs.

#ifndef CHROME_BROWSER_WEBDATA_WEB_DATA_SERVICE_H__
#define CHROME_BROWSER_WEBDATA_WEB_DATA_SERVICE_H__

#include <map>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/file_path.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/api/webdata/autofill_web_data_service.h"
#include "chrome/browser/api/webdata/web_data_results.h"
#include "chrome/browser/api/webdata/web_data_service_base.h"
#include "chrome/browser/api/webdata/web_data_service_consumer.h"
#include "chrome/browser/profiles/refcounted_profile_keyed_service.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_id.h"
#include "chrome/browser/webdata/keyword_table.h"
#include "chrome/browser/webdata/web_data_request_manager.h"
#include "content/public/browser/browser_thread.h"
#include "sql/init_status.h"

class AutocompleteSyncableService;
class AutofillChange;
class AutofillProfileSyncableService;
struct DefaultWebIntentService;
class GURL;
#if defined(OS_WIN)
struct IE7PasswordInfo;
#endif
class MessageLoop;
class Profile;
class SkBitmap;
class WebDatabase;

namespace base {
class Thread;
}

namespace webkit_glue {
struct WebIntentServiceData;
}

////////////////////////////////////////////////////////////////////////////////
//
// WebDataService is a generic data repository for meta data associated with
// web pages. All data is retrieved and archived in an asynchronous way.
//
// All requests return a handle. The handle can be used to cancel the request.
//
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
//
// WebDataService results
//
////////////////////////////////////////////////////////////////////////////////

typedef std::vector<AutofillChange> AutofillChangeList;

// Result from GetWebAppImages.
struct WDAppImagesResult {
  WDAppImagesResult();
  ~WDAppImagesResult();

  // True if SetWebAppHasAllImages(true) was invoked.
  bool has_all_images;

  // The images, may be empty.
  std::vector<SkBitmap> images;
};

struct WDKeywordsResult {
  WDKeywordsResult();
  ~WDKeywordsResult();

  KeywordTable::Keywords keywords;
  // Identifies the ID of the TemplateURL that is the default search. A value of
  // 0 indicates there is no default search provider.
  int64 default_search_provider_id;
  // Version of the built-in keywords. A value of 0 indicates a first run.
  int builtin_keyword_version;
};

class WebDataServiceConsumer;

class WebDataService
    : public WebDataServiceBase,
      public AutofillWebData,
      public RefcountedProfileKeyedService {
 public:
  WebDataService();

  // WebDataServiceBase implementation.
  virtual void CancelRequest(Handle h) OVERRIDE;
  virtual content::NotificationSource GetNotificationSource() OVERRIDE;

  // Notifies listeners on the UI thread that multiple changes have been made to
  // to Autofill records of the database.
  // NOTE: This method is intended to be called from the DB thread.  It
  // it asynchronously notifies listeners on the UI thread.
  // |web_data_service| may be NULL for testing purposes.
  static void NotifyOfMultipleAutofillChanges(WebDataService* web_data_service);

  // RefcountedProfileKeyedService override:
  // Shutdown the web data service. The service can no longer be used after this
  // call.
  virtual void ShutdownOnUIThread() OVERRIDE;

  // Initializes the web data service. Returns false on failure
  // Takes the path of the profile directory as its argument.
  bool Init(const FilePath& profile_path);

  // Returns false if Shutdown() has been called.
  bool IsRunning() const;

  // Unloads the database without actually shutting down the service.  This can
  // be used to temporarily reduce the browser process' memory footprint.
  void UnloadDatabase();

  virtual bool IsDatabaseLoaded();
  virtual WebDatabase* GetDatabase();

  //////////////////////////////////////////////////////////////////////////////
  //
  // Keywords
  //
  //////////////////////////////////////////////////////////////////////////////

  // As the database processes requests at a later date, all deletion is
  // done on the background thread.
  //
  // Many of the keyword related methods do not return a handle. This is because
  // the caller (TemplateURLService) does not need to know when the request is
  // done.

  void AddKeyword(const TemplateURLData& data);
  void RemoveKeyword(TemplateURLID id);
  void UpdateKeyword(const TemplateURLData& data);

  // Fetches the keywords.
  // On success, consumer is notified with WDResult<KeywordTable::Keywords>.
  Handle GetKeywords(WebDataServiceConsumer* consumer);

  // Sets the keywords used for the default search provider.
  void SetDefaultSearchProvider(const TemplateURL* url);

  // Sets the version of the builtin keywords.
  void SetBuiltinKeywordVersion(int version);

  //////////////////////////////////////////////////////////////////////////////
  //
  // Web Apps
  //
  //////////////////////////////////////////////////////////////////////////////

  // Sets the image for the specified web app. A web app can have any number of
  // images, but only one at a particular size. If there was an image for the
  // web app at the size of the given image it is replaced.
  void SetWebAppImage(const GURL& app_url, const SkBitmap& image);

  // Sets whether all the images have been downloaded for the specified web app.
  void SetWebAppHasAllImages(const GURL& app_url, bool has_all_images);

  // Removes all images for the specified web app.
  void RemoveWebApp(const GURL& app_url);

  // Fetches the images and whether all images have been downloaded for the
  // specified web app.
  Handle GetWebAppImages(const GURL& app_url, WebDataServiceConsumer* consumer);

  //////////////////////////////////////////////////////////////////////////////
  //
  // Web Intents
  //
  //////////////////////////////////////////////////////////////////////////////

  // Adds a web intent service registration.
  void AddWebIntentService(const webkit_glue::WebIntentServiceData& service);

  // Removes a web intent service registration.
  void RemoveWebIntentService(const webkit_glue::WebIntentServiceData& service);

  // Get all web intent services registered for the specified |action|.
  // |consumer| must not be NULL.
  Handle GetWebIntentServicesForAction(const string16& action,
                                       WebDataServiceConsumer* consumer);

  // Get all web intent services registered using the specified |service_url|.
  // |consumer| must not be NULL.
  Handle GetWebIntentServicesForURL(const string16& service_url,
                                    WebDataServiceConsumer* consumer);

  // Get all web intent services registered. |consumer| must not be NULL.
  Handle GetAllWebIntentServices(WebDataServiceConsumer* consumer);

  // Adds a default web intent service entry.
  void AddDefaultWebIntentService(const DefaultWebIntentService& service);

  // Removes a default web intent service entry. Removes entries matching
  // the |action|, |type|, and |url_pattern| of |service|.
  void RemoveDefaultWebIntentService(const DefaultWebIntentService& service);

  // Removes all default web intent service entries associated with
  // |service_url|
  void RemoveWebIntentServiceDefaults(const GURL& service_url);

    // Get a list of all web intent service defaults for the given |action|.
  // |consumer| must not be null.
  Handle GetDefaultWebIntentServicesForAction(const string16& action,
                                              WebDataServiceConsumer* consumer);

  // Get a list of all registered web intent service defaults.
  // |consumer| must not be null.
  Handle GetAllDefaultWebIntentServices(WebDataServiceConsumer* consumer);

  //////////////////////////////////////////////////////////////////////////////
  //
  // Token Service
  //
  //////////////////////////////////////////////////////////////////////////////

  // Set a token to use for a specified service.
  void SetTokenForService(const std::string& service,
                          const std::string& token);

  // Remove all tokens stored in the web database.
  void RemoveAllTokens();

  // Null on failure. Success is WDResult<std::vector<std::string> >
  virtual Handle GetAllTokens(WebDataServiceConsumer* consumer);

#if defined(OS_WIN)
  //////////////////////////////////////////////////////////////////////////////
  //
  // IE7/8 Password Access (used by PasswordStoreWin - do not use elsewhere)
  //
  //////////////////////////////////////////////////////////////////////////////

  // Adds |info| to the list of imported passwords from ie7/ie8.
  void AddIE7Login(const IE7PasswordInfo& info);

  // Removes |info| from the list of imported passwords from ie7/ie8.
  void RemoveIE7Login(const IE7PasswordInfo& info);

  // Get the login matching the information in |info|. |consumer| will be
  // notified when the request is done. The result is of type
  // WDResult<IE7PasswordInfo>.
  // If there is no match, the fields of the IE7PasswordInfo will be empty.
  Handle GetIE7Login(const IE7PasswordInfo& info,
                     WebDataServiceConsumer* consumer);
#endif  // defined(OS_WIN)

  //////////////////////////////////////////////////////////////////////////////
  //
  // Autofill.
  //
  //////////////////////////////////////////////////////////////////////////////

  // AutofillWebData implementation.
  virtual void AddFormFields(
      const std::vector<FormFieldData>& fields) OVERRIDE;
  virtual Handle GetFormValuesForElementName(
      const string16& name,
      const string16& prefix,
      int limit,
      WebDataServiceConsumer* consumer) OVERRIDE;
  virtual void RemoveExpiredFormElements() OVERRIDE;
  virtual void RemoveFormValueForElementName(const string16& name,
                                             const string16& value) OVERRIDE;
  virtual void AddAutofillProfile(const AutofillProfile& profile) OVERRIDE;
  virtual void UpdateAutofillProfile(const AutofillProfile& profile) OVERRIDE;
  virtual void RemoveAutofillProfile(const std::string& guid) OVERRIDE;
  virtual Handle GetAutofillProfiles(WebDataServiceConsumer* consumer) OVERRIDE;
  virtual void EmptyMigrationTrash(bool notify_sync) OVERRIDE;
  virtual void AddCreditCard(const CreditCard& credit_card) OVERRIDE;
  virtual void UpdateCreditCard(const CreditCard& credit_card) OVERRIDE;
  virtual void RemoveCreditCard(const std::string& guid) OVERRIDE;
  virtual Handle GetCreditCards(WebDataServiceConsumer* consumer) OVERRIDE;

  // Removes Autofill records from the database.
  void RemoveAutofillProfilesAndCreditCardsModifiedBetween(
      const base::Time& delete_begin,
      const base::Time& delete_end);

  // Removes form elements recorded for Autocomplete from the database.
  void RemoveFormElementsAddedBetween(const base::Time& delete_begin,
                                      const base::Time& delete_end);

  // Returns the syncable service for Autofill addresses and credit cards stored
  // in this table. The returned service is owned by |this| object.
  virtual AutofillProfileSyncableService*
      GetAutofillProfileSyncableService() const;

  // Returns the syncable service for field autocomplete stored in this table.
  // The returned service is owned by |this| object.
  virtual AutocompleteSyncableService*
      GetAutocompleteSyncableService() const;

  // Testing
#ifdef UNIT_TEST
  void set_failed_init(bool value) { failed_init_ = value; }
#endif

 protected:
  friend class TemplateURLServiceTest;
  friend class TemplateURLServiceTestingProfile;
  friend class WebDataServiceTest;
  friend class WebDataRequest;

  virtual ~WebDataService();

  // This is invoked by the unit test; path is the path of the Web Data file.
  bool InitWithPath(const FilePath& path);

  // Invoked by request implementations when a request has been processed.
  void RequestCompleted(Handle h);

  //////////////////////////////////////////////////////////////////////////////
  //
  // The following methods are only invoked in the web data service thread.
  //
  //////////////////////////////////////////////////////////////////////////////
 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<WebDataService>;

  typedef GenericRequest2<std::vector<const TemplateURLData*>,
                          KeywordTable::Keywords> SetKeywordsRequest;

  // Invoked on the main thread if initializing the db fails.
  void DBInitFailed(sql::InitStatus init_status);

  // Initialize the database, if it hasn't already been initialized.
  void InitializeDatabaseIfNecessary();

  // Initialize any syncable services.
  void InitializeSyncableServices();

  // The notification method.
  void NotifyDatabaseLoadedOnUIThread();

  // Commit any pending transaction and deletes the database.
  void ShutdownDatabase();

  // Deletes the syncable services.
  void ShutdownSyncableServices();

  // Commit the current transaction and creates a new one.
  void Commit();

  // Schedule a task on our worker thread.
  void ScheduleTask(const tracked_objects::Location& from_here,
                    const base::Closure& task);

  // Schedule a commit if one is not already pending.
  void ScheduleCommit();

  //////////////////////////////////////////////////////////////////////////////
  //
  // Keywords.
  //
  //////////////////////////////////////////////////////////////////////////////
  void AddKeywordImpl(GenericRequest<TemplateURLData>* request);
  void RemoveKeywordImpl(GenericRequest<TemplateURLID>* request);
  void UpdateKeywordImpl(GenericRequest<TemplateURLData>* request);
  void GetKeywordsImpl(WebDataRequest* request);
  void SetDefaultSearchProviderImpl(GenericRequest<TemplateURLID>* r);
  void SetBuiltinKeywordVersionImpl(GenericRequest<int>* r);

  //////////////////////////////////////////////////////////////////////////////
  //
  // Web Apps.
  //
  //////////////////////////////////////////////////////////////////////////////
  void SetWebAppImageImpl(GenericRequest2<GURL, SkBitmap>* request);
  void SetWebAppHasAllImagesImpl(GenericRequest2<GURL, bool>* request);
  void RemoveWebAppImpl(GenericRequest<GURL>* request);
  void GetWebAppImagesImpl(GenericRequest<GURL>* request);

  //////////////////////////////////////////////////////////////////////////////
  //
  // Web Intents.
  //
  //////////////////////////////////////////////////////////////////////////////
  void AddWebIntentServiceImpl(
      GenericRequest<webkit_glue::WebIntentServiceData>* request);
  void RemoveWebIntentServiceImpl(
      GenericRequest<webkit_glue::WebIntentServiceData>* request);
  void GetWebIntentServicesImpl(GenericRequest<string16>* request);
  void GetWebIntentServicesForURLImpl(GenericRequest<string16>* request);
  void GetAllWebIntentServicesImpl(GenericRequest<std::string>* request);
  void AddDefaultWebIntentServiceImpl(
      GenericRequest<DefaultWebIntentService>* request);
  void RemoveDefaultWebIntentServiceImpl(
      GenericRequest<DefaultWebIntentService>* request);
  void RemoveWebIntentServiceDefaultsImpl(GenericRequest<GURL>* request);
  void GetDefaultWebIntentServicesForActionImpl(
      GenericRequest<string16>* request);
  void GetAllDefaultWebIntentServicesImpl(GenericRequest<std::string>* request);

  //////////////////////////////////////////////////////////////////////////////
  //
  // Token Service.
  //
  //////////////////////////////////////////////////////////////////////////////

  void RemoveAllTokensImpl(GenericRequest<std::string>* request);
  void SetTokenForServiceImpl(
    GenericRequest2<std::string, std::string>* request);
  void GetAllTokensImpl(GenericRequest<std::string>* request);

#if defined(OS_WIN)
  //////////////////////////////////////////////////////////////////////////////
  //
  // Password manager.
  //
  //////////////////////////////////////////////////////////////////////////////
  void AddIE7LoginImpl(GenericRequest<IE7PasswordInfo>* request);
  void RemoveIE7LoginImpl(GenericRequest<IE7PasswordInfo>* request);
  void GetIE7LoginImpl(GenericRequest<IE7PasswordInfo>* request);
#endif  // defined(OS_WIN)

  //////////////////////////////////////////////////////////////////////////////
  //
  // Autofill.
  //
  //////////////////////////////////////////////////////////////////////////////
  void AddFormElementsImpl(
      GenericRequest<std::vector<FormFieldData> >* request);
  void GetFormValuesForElementNameImpl(WebDataRequest* request,
      const string16& name, const string16& prefix, int limit);
  void RemoveFormElementsAddedBetweenImpl(
      GenericRequest2<base::Time, base::Time>* request);
  void RemoveExpiredFormElementsImpl(WebDataRequest* request);
  void RemoveFormValueForElementNameImpl(
      GenericRequest2<string16, string16>* request);
  void AddAutofillProfileImpl(GenericRequest<AutofillProfile>* request);
  void UpdateAutofillProfileImpl(GenericRequest<AutofillProfile>* request);
  void RemoveAutofillProfileImpl(GenericRequest<std::string>* request);
  void GetAutofillProfilesImpl(WebDataRequest* request);
  void EmptyMigrationTrashImpl(GenericRequest<bool>* request);
  void AddCreditCardImpl(GenericRequest<CreditCard>* request);
  void UpdateCreditCardImpl(GenericRequest<CreditCard>* request);
  void RemoveCreditCardImpl(GenericRequest<std::string>* request);
  void GetCreditCardsImpl(WebDataRequest* request);
  void RemoveAutofillProfilesAndCreditCardsModifiedBetweenImpl(
      GenericRequest2<base::Time, base::Time>* request);

  // Callbacks to ensure that sensitive info is destroyed if request is
  // cancelled.
  void DestroyAutofillProfileResult(const WDTypedResult* result);
  void DestroyAutofillCreditCardResult(const WDTypedResult* result);

  // True once initialization has started.
  bool is_running_;

  // The path with which to initialize the database.
  FilePath path_;

  // Our database.  We own the |db_|, but don't use a |scoped_ptr| because the
  // |db_| lifetime must be managed on the database thread.
  WebDatabase* db_;

  // Keeps track of all pending requests made to the db.
  WebDataRequestManager request_manager_;

  // The application locale.  The locale is needed for some database migrations,
  // and must be read on the UI thread.  It's cached here so that we can pass it
  // to the migration code on the DB thread.
  const std::string app_locale_;

  // Syncable services for the database data.  We own the services, but don't
  // use |scoped_ptr|s because the lifetimes must be managed on the database
  // thread.
  // Currently only Autocomplete and Autofill profiles use the new Sync API, but
  // all the database data should migrate to this API over time.
  AutocompleteSyncableService* autocomplete_syncable_service_;
  AutofillProfileSyncableService* autofill_profile_syncable_service_;

  // Whether the database failed to initialize.  We use this to avoid
  // continually trying to reinit.
  bool failed_init_;

  // Whether we should commit the database.
  bool should_commit_;

  // MessageLoop the WebDataService is created on.
  MessageLoop* main_loop_;

  DISALLOW_COPY_AND_ASSIGN(WebDataService);
};

#endif  // CHROME_BROWSER_WEBDATA_WEB_DATA_SERVICE_H__
