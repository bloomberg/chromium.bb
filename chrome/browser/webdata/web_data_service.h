// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBDATA_WEB_DATA_SERVICE_H__
#define CHROME_BROWSER_WEBDATA_WEB_DATA_SERVICE_H__
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/search_engines/template_url_id.h"
#include "content/public/browser/browser_thread.h"
#include "sql/init_status.h"

class AutocompleteSyncableService;
class AutofillChange;
class AutofillProfile;
class AutofillProfileSyncableService;
class CreditCard;
class GURL;
#if defined(OS_WIN)
struct IE7PasswordInfo;
#endif
class MessageLoop;
class Profile;
class SkBitmap;
class TemplateURL;
class WebDatabase;

namespace base {
class Thread;
}

namespace webkit_glue {
struct FormField;
struct PasswordForm;
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

//
// Result types
//
typedef enum {
  BOOL_RESULT = 1,             // WDResult<bool>
  KEYWORDS_RESULT,             // WDResult<WDKeywordsResult>
  INT64_RESULT,                // WDResult<int64>
  PASSWORD_RESULT,             // WDResult<std::vector<PasswordForm*>>
#if defined(OS_WIN)
  PASSWORD_IE7_RESULT,         // WDResult<IE7PasswordInfo>
#endif
  WEB_APP_IMAGES,              // WDResult<WDAppImagesResult>
  TOKEN_RESULT,                // WDResult<std::vector<std::string>>
  AUTOFILL_VALUE_RESULT,       // WDResult<std::vector<string16>>
  AUTOFILL_CHANGES,            // WDResult<std::vector<AutofillChange>>
  AUTOFILL_PROFILE_RESULT,     // WDResult<AutofillProfile>
  AUTOFILL_PROFILES_RESULT,    // WDResult<std::vector<AutofillProfile*>>
  AUTOFILL_CREDITCARD_RESULT,  // WDResult<CreditCard>
  AUTOFILL_CREDITCARDS_RESULT, // WDResult<std::vector<CreditCard*>>
  WEB_INTENTS_RESULT           // WDResult<std::vector<string16>>
} WDResultType;

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

  std::vector<TemplateURL*> keywords;
  // Identifies the ID of the TemplateURL that is the default search. A value of
  // 0 indicates there is no default search provider.
  int64 default_search_provider_id;
  // Version of the built-in keywords. A value of 0 indicates a first run.
  int builtin_keyword_version;
  // Backup of the default search provider ID.
  int64 default_search_provider_id_backup;
  // Indicates if default search provider has been changed by something
  // other than user's action in the browser.
  bool did_default_search_provider_change;
};

//
// The top level class for a result.
//
class WDTypedResult {
 public:
  virtual ~WDTypedResult() {}

  // Return the result type.
  WDResultType GetType() const {
    return type_;
  }

 protected:
  explicit WDTypedResult(WDResultType type) : type_(type) {
  }

 private:
  WDResultType type_;
  DISALLOW_COPY_AND_ASSIGN(WDTypedResult);
};

// A result containing one specific pointer or literal value.
template <class T> class WDResult : public WDTypedResult {
 public:

  WDResult(WDResultType type, const T& v) : WDTypedResult(type), value_(v) {
  }

  virtual ~WDResult() {
  }

  // Return a single value result.
  T GetValue() const {
    return value_;
  }

 private:
  T value_;

  DISALLOW_COPY_AND_ASSIGN(WDResult);
};

template <class T> class WDObjectResult : public WDTypedResult {
 public:
  explicit WDObjectResult(WDResultType type) : WDTypedResult(type) {
  }

  T* GetValue() const {
    return &value_;
  }

 private:
  // mutable to keep GetValue() const.
  mutable T value_;
  DISALLOW_COPY_AND_ASSIGN(WDObjectResult);
};

class WebDataServiceConsumer;

class WebDataService
    : public base::RefCountedThreadSafe<
          WebDataService, content::BrowserThread::DeleteOnUIThread> {
 public:
  // All requests return an opaque handle of the following type.
  typedef int Handle;

  //////////////////////////////////////////////////////////////////////////////
  //
  // Internal requests
  //
  // Every request is processed using a request object. The object contains
  // both the request parameters and the results.
  //////////////////////////////////////////////////////////////////////////////
  class WebDataRequest {
   public:
    WebDataRequest(WebDataService* service,
                   Handle handle,
                   WebDataServiceConsumer* consumer);

    virtual ~WebDataRequest();

    Handle GetHandle() const;

    // Retrieves the |consumer_| set in the constructor.  If the request was
    // cancelled via the |Cancel()| method then |true| is returned and
    // |*consumer| is set to NULL.  The |consumer| parameter may be set to NULL
    // if only the return value is desired.
    bool IsCancelled(WebDataServiceConsumer** consumer) const;

    // This can be invoked from any thread. From this point we assume that
    // our consumer_ reference is invalid.
    void Cancel();

    // Invoked by the service when this request has been completed.
    // This will notify the service in whatever thread was used to create this
    // request.
    void RequestComplete();

    // The result is owned by the request.
    void SetResult(WDTypedResult* r);
    const WDTypedResult* GetResult() const;

   private:
    scoped_refptr<WebDataService> service_;
    MessageLoop* message_loop_;
    Handle handle_;

    // A lock to protect against simultaneous cancellations of the request.
    // Cancellation affects both the |cancelled_| flag and |consumer_|.
    mutable base::Lock cancel_lock_;
    bool cancelled_;
    WebDataServiceConsumer* consumer_;

    WDTypedResult* result_;

    DISALLOW_COPY_AND_ASSIGN(WebDataRequest);
  };

  //
  // Internally we use instances of the following template to represent
  // requests.
  //
  template <class T>
  class GenericRequest : public WebDataRequest {
   public:
    GenericRequest(WebDataService* service,
                   Handle handle,
                   WebDataServiceConsumer* consumer,
                   const T& arg)
        : WebDataRequest(service, handle, consumer),
          arg_(arg) {
    }

    virtual ~GenericRequest() {
    }

    const T& arg() const { return arg_; }

   private:
    T arg_;
  };

  template <class T, class U>
  class GenericRequest2 : public WebDataRequest {
   public:
    GenericRequest2(WebDataService* service,
                    Handle handle,
                    WebDataServiceConsumer* consumer,
                    const T& arg1,
                    const U& arg2)
        : WebDataRequest(service, handle, consumer),
          arg1_(arg1),
          arg2_(arg2) {
    }

    virtual ~GenericRequest2() { }

    const T& arg1() const { return arg1_; }

    const U& arg2() const { return arg2_; }

   private:
    T arg1_;
    U arg2_;
  };

  WebDataService();

  // Notifies listeners on the UI thread that multiple changes have been made to
  // to Autofill records of the database.
  // NOTE: This method is intended to be called from the DB thread.  It
  // it asynchronously notifies listeners on the UI thread.
  // |web_data_service| may be NULL for testing purposes.
  static void NotifyOfMultipleAutofillChanges(WebDataService* web_data_service);

  // Initializes the web data service. Returns false on failure
  // Takes the path of the profile directory as its argument.
  bool Init(const FilePath& profile_path);

  // Shutdown the web data service. The service can no longer be used after this
  // call.
  void Shutdown();

  // Returns false if Shutdown() has been called.
  bool IsRunning() const;

  // Unloads the database without actually shutting down the service.  This can
  // be used to temporarily reduce the browser process' memory footprint.
  void UnloadDatabase();

  // Cancel any pending request. You need to call this method if your
  // WebDataServiceConsumer is about to be deleted.
  void CancelRequest(Handle h);

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
  void AddKeyword(const TemplateURL& url);

  void RemoveKeyword(const TemplateURL& url);

  void UpdateKeyword(const TemplateURL& url);

  // Fetches the keywords.
  // On success, consumer is notified with WDResult<std::vector<TemplateURL*>.
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
  Handle GetWebIntentServices(const string16& action,
                              WebDataServiceConsumer* consumer);

  // Get all web intent services registered using the specified |service_url|.
  // |consumer| must not be NULL.
  Handle GetWebIntentServicesForURL(const string16& service_url,
                                    WebDataServiceConsumer* consumer);

  // Get all web intent services registered. |consumer| must not be NULL.
  Handle GetAllWebIntentServices(WebDataServiceConsumer* consumer);

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
  Handle GetAllTokens(WebDataServiceConsumer* consumer);

  //////////////////////////////////////////////////////////////////////////////
  //
  // Password manager
  // NOTE: These methods are all deprecated; new clients should use
  // PasswordStore. These are only still here because Windows is (temporarily)
  // still using them for its PasswordStore implementation.
  //
  //////////////////////////////////////////////////////////////////////////////

  // Adds |form| to the list of remembered password forms.
  void AddLogin(const webkit_glue::PasswordForm& form);

  // Updates the remembered password form.
  void UpdateLogin(const webkit_glue::PasswordForm& form);

  // Removes |form| from the list of remembered password forms.
  void RemoveLogin(const webkit_glue::PasswordForm& form);

  // Removes all logins created in the specified daterange
  void RemoveLoginsCreatedBetween(const base::Time& delete_begin,
                                  const base::Time& delete_end);

  // Removes all logins created on or after the date passed in.
  void RemoveLoginsCreatedAfter(const base::Time& delete_begin);

  // Gets a list of password forms that match |form|.
  // |consumer| will be notified when the request is done. The result is of
  // type WDResult<std::vector<PasswordForm*>>.
  // The result will be null on failure. The |consumer| owns all PasswordForm's.
  Handle GetLogins(const webkit_glue::PasswordForm& form,
                   WebDataServiceConsumer* consumer);

  // Gets the complete list of password forms that have not been blacklisted and
  // are thus auto-fillable.
  // |consumer| will be notified when the request is done. The result is of
  // type WDResult<std::vector<PasswordForm*>>.
  // The result will be null on failure.  The |consumer| owns all PasswordForms.
  Handle GetAutofillableLogins(WebDataServiceConsumer* consumer);

  // Gets the complete list of password forms that have been blacklisted.
  // |consumer| will be notified when the request is done. The result is of
  // type WDResult<std::vector<PasswordForm*>>.
  // The result will be null on failure. The |consumer| owns all PasswordForm's.
  Handle GetBlacklistLogins(WebDataServiceConsumer* consumer);

#if defined(OS_WIN)
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

  // Schedules a task to add form fields to the web database.
  virtual void AddFormFields(const std::vector<webkit_glue::FormField>& fields);

  // Initiates the request for a vector of values which have been entered in
  // form input fields named |name|.  The method OnWebDataServiceRequestDone of
  // |consumer| gets called back when the request is finished, with the vector
  // included in the argument |result|.
  Handle GetFormValuesForElementName(const string16& name,
                                     const string16& prefix,
                                     int limit,
                                     WebDataServiceConsumer* consumer);

  // Removes form elements recorded for Autocomplete from the database.
  void RemoveFormElementsAddedBetween(const base::Time& delete_begin,
                                      const base::Time& delete_end);
  void RemoveFormValueForElementName(const string16& name,
                                     const string16& value);

  // Schedules a task to add an Autofill profile to the web database.
  void AddAutofillProfile(const AutofillProfile& profile);

  // Schedules a task to update an Autofill profile in the web database.
  void UpdateAutofillProfile(const AutofillProfile& profile);

  // Schedules a task to remove an Autofill profile from the web database.
  // |guid| is the identifer of the profile to remove.
  void RemoveAutofillProfile(const std::string& guid);

  // Initiates the request for all Autofill profiles.  The method
  // OnWebDataServiceRequestDone of |consumer| gets called when the request is
  // finished, with the profiles included in the argument |result|.  The
  // consumer owns the profiles.
  Handle GetAutofillProfiles(WebDataServiceConsumer* consumer);

  // Remove "trashed" profile guids from the web database and optionally send
  // notifications to tell Sync that the items have been removed.
  void EmptyMigrationTrash(bool notify_sync);

  // Schedules a task to add credit card to the web database.
  void AddCreditCard(const CreditCard& credit_card);

  // Schedules a task to update credit card in the web database.
  void UpdateCreditCard(const CreditCard& credit_card);

  // Schedules a task to remove a credit card from the web database.
  // |guid| is identifer of the credit card to remove.
  void RemoveCreditCard(const std::string& guid);

  // Initiates the request for all credit cards.  The method
  // OnWebDataServiceRequestDone of |consumer| gets called when the request is
  // finished, with the credit cards included in the argument |result|.  The
  // consumer owns the credit cards.
  Handle GetCreditCards(WebDataServiceConsumer* consumer);

  // Removes Autofill records from the database.
  void RemoveAutofillProfilesAndCreditCardsModifiedBetween(
      const base::Time& delete_begin,
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

  // Register the request as a pending request.
  void RegisterRequest(WebDataRequest* request);

  //////////////////////////////////////////////////////////////////////////////
  //
  // The following methods are only invoked in the web data service thread.
  //
  //////////////////////////////////////////////////////////////////////////////
 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class DeleteTask<WebDataService>;
  friend class ShutdownTask;

  typedef GenericRequest2<std::vector<const TemplateURL*>,
                          std::vector<TemplateURL*> > SetKeywordsRequest;

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
  void ScheduleTask(const base::Closure& task);

  // Schedule a commit if one is not already pending.
  void ScheduleCommit();

  // Return the next request handle.
  int GetNextRequestHandle();

  //////////////////////////////////////////////////////////////////////////////
  //
  // Keywords.
  //
  //////////////////////////////////////////////////////////////////////////////
  void AddKeywordImpl(GenericRequest<TemplateURL>* request);
  void RemoveKeywordImpl(GenericRequest<TemplateURLID>* request);
  void UpdateKeywordImpl(GenericRequest<TemplateURL>* request);
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

  //////////////////////////////////////////////////////////////////////////////
  //
  // Token Service.
  //
  //////////////////////////////////////////////////////////////////////////////

  void RemoveAllTokensImpl(GenericRequest<std::string>* request);
  void SetTokenForServiceImpl(
    GenericRequest2<std::string, std::string>* request);
  void GetAllTokensImpl(GenericRequest<std::string>* request);

  //////////////////////////////////////////////////////////////////////////////
  //
  // Password manager.
  //
  //////////////////////////////////////////////////////////////////////////////
  void AddLoginImpl(GenericRequest<webkit_glue::PasswordForm>* request);
  void UpdateLoginImpl(GenericRequest<webkit_glue::PasswordForm>* request);
  void RemoveLoginImpl(GenericRequest<webkit_glue::PasswordForm>* request);
  void RemoveLoginsCreatedBetweenImpl(
      GenericRequest2<base::Time, base::Time>* request);
  void GetLoginsImpl(GenericRequest<webkit_glue::PasswordForm>* request);
  void GetAutofillableLoginsImpl(WebDataRequest* request);
  void GetBlacklistLoginsImpl(WebDataRequest* request);
#if defined(OS_WIN)
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
      GenericRequest<std::vector<webkit_glue::FormField> >* request);
  void GetFormValuesForElementNameImpl(WebDataRequest* request,
      const string16& name, const string16& prefix, int limit);
  void RemoveFormElementsAddedBetweenImpl(
      GenericRequest2<base::Time, base::Time>* request);
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

  // True once initialization has started.
  bool is_running_;

  // The path with which to initialize the database.
  FilePath path_;

  // Our database.  We own the |db_|, but don't use a |scoped_ptr| because the
  // |db_| lifetime must be managed on the database thread.
  WebDatabase* db_;

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

  // A lock to protect pending requests and next request handle.
  base::Lock pending_lock_;

  // Next handle to be used for requests. Incremented for each use.
  Handle next_request_handle_;

  typedef std::map<Handle, WebDataRequest*> RequestMap;
  RequestMap pending_requests_;

  // MessageLoop the WebDataService is created on.
  MessageLoop* main_loop_;

  DISALLOW_COPY_AND_ASSIGN(WebDataService);
};

////////////////////////////////////////////////////////////////////////////////
//
// WebDataServiceConsumer.
//
// All requests to the web data service are asynchronous. When the request has
// been performed, the data consumer is notified using the following interface.
//
////////////////////////////////////////////////////////////////////////////////

class WebDataServiceConsumer {
 public:

  // Called when a request is done. h uniquely identifies the request.
  // result can be NULL, if no result is expected or if the database could
  // not be opened. The result object is destroyed after this call.
  virtual void OnWebDataServiceRequestDone(WebDataService::Handle h,
                                           const WDTypedResult* result) = 0;

 protected:
  virtual ~WebDataServiceConsumer() {}
};

#endif  // CHROME_BROWSER_WEBDATA_WEB_DATA_SERVICE_H__
