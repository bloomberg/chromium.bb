// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_FILE_SYSTEM_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_FILE_SYSTEM_H_

#include <map>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop_proxy.h"
#include "base/platform_file.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/chromeos/gdata/gdata.h"
#include "chrome/browser/chromeos/gdata/gdata_parser.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace gdata {

class DocumentsService;
class GDataDirectory;
class GDataFile;

// Base class for representing files and directories in gdata virtual file
// system.
class GDataFileBase {
 public:
  explicit GDataFileBase(GDataDirectory* parent);
  virtual ~GDataFileBase();
  // Converts DocumentEntry into GDataFileBase.
  static GDataFileBase* FromDocumentEntry(GDataDirectory* parent,
                                          DocumentEntry* doc);
  virtual GDataFile* AsGDataFile();
  virtual GDataDirectory* AsGDataDirectory();
  GDataDirectory* parent() { return parent_; }
  const base::PlatformFileInfo& file_info() const { return file_info_; }
  const FilePath::StringType& file_name() const { return file_name_; }
  const FilePath::StringType& original_file_name() const {
    return original_file_name_;
  }
  void set_file_name(const FilePath::StringType& name) { file_name_ = name; }

  // The content URL is used for downloading regular files as is.
  const GURL& content_url() const { return content_url_; }

  // The self URL is used for removing files and hosted documents.
  const GURL& self_url() const { return self_url_; }

  // Returns virtual file path representing this file system entry. This path
  // corresponds to file path expected by public methods of GDataFileSyste
  // class.
  FilePath GetFilePath();

 protected:
  base::PlatformFileInfo file_info_;
  FilePath::StringType file_name_;
  FilePath::StringType original_file_name_;
  // Files with the same original name will be uniquely identified with this
  // field so we can represent them with unique URLs/paths in File API layer.
  // For example, two files in the same directory with the same name "Foo"
  // will show up in the virtual directory as "Foo" and "Foo (2)".
  GURL self_url_;
  GURL content_url_;
  GDataDirectory* parent_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GDataFileBase);
};

typedef std::map<FilePath::StringType, GDataFileBase*> GDataFileCollection;

// Represents "file" in in a GData virtual file system. On gdata feed side,
// this could be either a regular file or a server side document.
class GDataFile : public GDataFileBase {
 public:
  // This is used as a bitmask for the cache state.
  enum CacheState {
    CACHE_STATE_NONE    = 0x0,
    CACHE_STATE_PINNED  = 0x1 << 0,
    CACHE_STATE_PRESENT = 0x1 << 1,
    CACHE_STATE_DIRTY   = 0x1 << 2,
  };

  explicit GDataFile(GDataDirectory* parent);
  virtual ~GDataFile();
  virtual GDataFile* AsGDataFile() OVERRIDE;

  static GDataFileBase* FromDocumentEntry(GDataDirectory* parent,
                                          DocumentEntry* doc);

  DocumentEntry::EntryKind kind() const { return kind_; }
  const GURL& thumbnail_url() const { return thumbnail_url_; }
  const GURL& edit_url() const { return edit_url_; }
  const std::string& content_mime_type() const { return content_mime_type_; }
  const std::string& etag() const { return etag_; }
  const std::string& resource() const { return resource_id_; }
  const std::string& id() const { return id_; }
  const std::string& file_md5() const { return file_md5_; }
  // Returns a bitmask of CacheState enum values.
  const int cache_state() const { return cache_state_; }

 private:
  // Content URL for files.
  DocumentEntry::EntryKind kind_;
  GURL thumbnail_url_;
  GURL edit_url_;
  std::string content_mime_type_;
  std::string etag_;
  std::string resource_id_;
  std::string id_;
  std::string file_md5_;
  int cache_state_;

  DISALLOW_COPY_AND_ASSIGN(GDataFile);
};

// Represents "directory" in a GData virtual file system. Maps to gdata
// collection element.
class GDataDirectory : public GDataFileBase {
 public:
  explicit GDataDirectory(GDataDirectory* parent);
  virtual ~GDataDirectory();
  virtual GDataDirectory* AsGDataDirectory() OVERRIDE;

  static GDataFileBase* FromDocumentEntry(GDataDirectory* parent,
                                          DocumentEntry* doc);

  // Adds child file to the directory and takes over the ownership of |file|
  // object. The method will also do name deduplication to ensure that the
  // exposed presentation path does not have naming conflicts. Two files with
  // the same name "Foo" will be renames to "Foo (1)" and "Foo (2)".
  void AddFile(GDataFileBase* file);

  // Removes the file from its children list.
  bool RemoveFile(GDataFileBase* file);

  // Checks if directory content needs to be retrieved again. If it does,
  // the function will return URL for next feed in |next_feed_url|.
  bool NeedsRefresh(GURL* next_feed_url);

  // Removes children elements.
  void RemoveChildren();

  // Last refresh time.
  const base::Time& refresh_time() const { return refresh_time_; }
  void set_refresh_time(const base::Time& time) { refresh_time_ = time; }
  // Url for this feed.
  const GURL& start_feed_url() const { return start_feed_url_; }
  void set_start_feed_url(const GURL& url) { start_feed_url_ = url; }
  // Continuing feed's url.
  const GURL& next_feed_url() const { return next_feed_url_; }
  void set_next_feed_url(const GURL& url) { next_feed_url_ = url; }
  // Collection of children GDataFileBase items.
  const GDataFileCollection& children() const { return children_; }

 private:
  base::Time refresh_time_;
  // Url for this feed.
  GURL start_feed_url_;
  // Continuing feed's url.
  GURL next_feed_url_;
  // Collection of children GDataFileBase items.
  GDataFileCollection children_;

  DISALLOW_COPY_AND_ASSIGN(GDataDirectory);
};

// Delegate class used to deal with results of virtual directory request
// to FindFileByPath() method. This class is refcounted since we pass it
// around and access it from different threads.
class FindFileDelegate : public base::RefCountedThreadSafe<FindFileDelegate> {
 public:
  virtual ~FindFileDelegate();

  enum FindFileTraversalCommand {
    FIND_FILE_CONTINUES,
    FIND_FILE_TERMINATES,
  };

  // Called when |file| search is completed within the file system.
  virtual void OnFileFound(GDataFile* file) = 0;

  // Called when |directory| is found at |directory_path}| within the file
  // system.
  virtual void OnDirectoryFound(const FilePath& directory_path,
                                GDataDirectory* directory) = 0;

  // Called while traversing the virtual file system when |directory|
  // under |directory_path| is encountered. If this function returns
  // FIND_FILE_TERMINATES the current find operation will be terminated.
  virtual FindFileTraversalCommand OnEnterDirectory(
      const FilePath& directory_path, GDataDirectory* directory) = 0;

  // Called when an error occurs while fetching feed content from the server.
  virtual void OnError(base::PlatformFileError error) = 0;
};

// Delegate used to find a directory element for file system updates.
class ReadOnlyFindFileDelegate : public FindFileDelegate {
 public:
  ReadOnlyFindFileDelegate();

  // Returns found file.
  GDataFileBase* file() { return file_; }

 private:
  // GDataFileSystem::FindFileDelegate overrides.
  virtual void OnFileFound(gdata::GDataFile* file) OVERRIDE;
  virtual void OnDirectoryFound(const FilePath&,
                                GDataDirectory* dir) OVERRIDE;
  virtual FindFileTraversalCommand OnEnterDirectory(const FilePath&,
                                                    GDataDirectory*) OVERRIDE;
  virtual void OnError(base::PlatformFileError) OVERRIDE;

  // File entry that was found.
  GDataFileBase* file_;
};


// GData file system abstraction layer.
// GDataFileSystem is per-profie, hence inheriting ProfileKeyedService.
class GDataFileSystem : public ProfileKeyedService {
 public:
  struct FindFileParams {
    FindFileParams(const FilePath& in_file_path,
                   bool in_require_content,
                   const FilePath& in_directory_path,
                   const GURL& in_feed_url,
                   bool in_initial_feed,
                   scoped_refptr<FindFileDelegate> in_delegate);
    ~FindFileParams();

    const FilePath file_path;
    const bool require_content;
    const FilePath directory_path;
    const GURL feed_url;
    const bool initial_feed;
    const scoped_refptr<FindFileDelegate> delegate;
  };

  // Used for file operations like removing files.
  typedef base::Callback<void(base::PlatformFileError error)>
      FileOperationCallback;

  // Used to get files from the file system.
  typedef base::Callback<void(base::PlatformFileError error,
                              const FilePath& file_path)>
      GetFileCallback;

  // ProfileKeyedService override:
  virtual void Shutdown() OVERRIDE;

  // Authenticates the user by fetching the auth token as
  // needed. |callback| will be run with the error code and the auth
  // token, on the thread this function is run.
  //
  // Must be called on UI thread.
  void Authenticate(const AuthStatusCallback& callback);

  // Finds file info by using virtual |file_path|. If |require_content| is set,
  // the found directory will be pre-populated before passed back to the
  // |delegate|. If |allow_refresh| is not set, directories' content
  // won't be performed.
  //
  // Can be called from any thread.
  void FindFileByPath(const FilePath& file_path,
                      scoped_refptr<FindFileDelegate> delegate);

  // Removes |file_path| from the file system.  If |is_recursive| is set and
  // |file_path| represents a directory, we will also delete all of its
  // contained children elements. The file entry represented by |file_path|
  // needs to be present in in-memory representation of the file system that
  // in order to be removed.
  //
  // TODO(zelidrag): Wire |is_recursive| through gdata api
  // (find appropriate calls for it).
  //
  // Can be called from any thread. |callback| is run on the calling thread.
  void Remove(const FilePath& file_path,
              bool is_recursive,
              const FileOperationCallback& callback);

  // Creates new directory under |directory_path|. If |is_exclusive| is true,
  // an error is raised in case a directory is already present at the
  // |directory_path|. If |is_recursive| is true, the call creates parent
  // directories as needed just like mkdir -p does.
  //
  // Can be called from any thread. |callback| is run on the calling thread.
  void CreateDirectory(const FilePath& directory_path,
                       bool is_exclusive,
                       bool is_recursive,
                       const FileOperationCallback& callback);

  // Gets |file_path| from the file system. The file entry represented by
  // |file_path| needs to be present in in-memory representation of the file
  // system in order to be retrieved. If the file is not cached, the file
  // will be downloaded through gdata api.
  //
  // Can be called from any thread. |callback| is run on the calling thread.
  void GetFile(const FilePath& file_path, const GetFileCallback& callback);

  // initiated FindFileByPath() attempt upon its completion. Safe to be called
  // from any thread. Internally, it will route content refresh request to
  // DocumentsService::GetDocuments() which will initiated content
  // fetching from UI thread as required by gdata library (UrlFetcher).
  //
  // Can be called from any thread.
  void StartDirectoryRefresh(const FindFileParams& params);

  // Finds file object by |file_path| and returns the file info.
  // Returns NULL if it does not find the file.
  GDataFileBase* GetGDataFileInfoFromPath(const FilePath& file_path);

 private:
  friend class GDataFileSystemFactory;
  friend class GDataFileSystemTest;
  FRIEND_TEST_ALL_PREFIXES(GDataFileSystemTest,
                           FindFirstMissingParentDirectory);

  // Defines possible search results of FindFirstMissingParentDirectory().
  enum FindMissingDirectoryResult {
    // Target directory found, it's not a directory.
    FOUND_INVALID,
    // Found missing directory segment while searching for given directory.
    FOUND_MISSING,
    // Found target directory, it already exists.
    DIRECTORY_ALREADY_PRESENT,
  };

  // Defines set of parameters passes to intermediate callbacks during
  // execution of CreateDirectory() method.
  struct CreateDirectoryParams {
    CreateDirectoryParams(const FilePath& created_directory_path,
                          const FilePath& target_directory_path,
                          bool is_exclusive,
                          bool is_recursive,
                          const FileOperationCallback& callback,
                          scoped_refptr<base::MessageLoopProxy> proxy);
    ~CreateDirectoryParams();

    const FilePath created_directory_path;
    const FilePath target_directory_path;
    const bool is_exclusive;
    const bool is_recursive;
    FileOperationCallback callback;
    scoped_refptr<base::MessageLoopProxy> proxy;
  };

  explicit GDataFileSystem(Profile* profile);
  virtual ~GDataFileSystem();

  // Unsafe (unlocked) version of the function above.
  void UnsafeFindFileByPath(const FilePath& file_path,
                            scoped_refptr<FindFileDelegate> delegate);

  // Converts document feed from gdata service into DirectoryInfo. On failure,
  // returns NULL and fills in |error| with an appropriate value.
  GDataDirectory* ParseGDataFeed(GDataErrorCode status,
                                 base::Value* data,
                                 base::PlatformFileError *error);

  // Callback for handling feed content fetching while searching for file info.
  // This callback is invoked after async feed fetch operation that was
  // invoked by StartDirectoryRefresh() completes. This callback will update
  // the content of the refreshed directory object and continue initially
  // started FindFileByPath() request.
  void OnGetDocuments(const FindFileParams& params,
                      GDataErrorCode status,
                      base::Value* data);

  // Callback for handling document remove attempt. |message_loop_proxy| is
  // used to post a task to the thread where Remove() was called.
  void OnRemovedDocument(
      const FileOperationCallback& callback,
      const FilePath& file_path,
      scoped_refptr<base::MessageLoopProxy> message_loop_proxy,
      GDataErrorCode status,
      const GURL& document_url);

  // Callback for handling directory create requests.
  void OnCreateDirectoryCompleted(
      const CreateDirectoryParams& params,
      GDataErrorCode status,
      base::Value* created_entry);

  // Callback for handling file downloading requests.
  void OnFileDownloaded(
    const GetFileCallback& callback,
    scoped_refptr<base::MessageLoopProxy> message_loop_proxy,
    GDataErrorCode status,
    const GURL& content_url,
    const FilePath& temp_file);

  // Removes file under |file_path| from in-memory snapshot of the file system.
  // Return PLATFORM_FILE_OK if successful.
  base::PlatformFileError RemoveFileFromFileSystem(const FilePath& file_path);

  // Updates content of the directory identified with |directory_path|. If the
  // feed was not complete, it will return URL for the remaining portion in
  // |next_feed|. On success, returns PLATFORM_FILE_OK.
  base::PlatformFileError UpdateDirectoryWithDocumentFeed(
      const FilePath& directory_path,
      const GURL& feed_url,
      base::Value* data,
      bool is_initial_feed,
      GURL* next_feed);

  // Converts |entry_value| into GFileDocument instance and adds it
  // to virtual file system at |directory_path|.
  base::PlatformFileError AddNewDirectory(const FilePath& directory_path,
                                          base::Value* entry_value);

  // Internal version of CreateDirectory() that let us pass explicit message
  // loop proxy.
  void CreateDirectoryInternal(
      const FilePath& directory_path,
      bool is_exclusive,
      bool is_recursive,
      const FileOperationCallback& callback,
      scoped_refptr<base::MessageLoopProxy> reply_proxy);

  // Given non-existing |directory_path|, finds the first missing parent
  // directory of |directory_path|.
  FindMissingDirectoryResult FindFirstMissingParentDirectory(
      const FilePath& directory_path,
      GURL* last_dir_content_url,
      FilePath* first_missing_parent_path);

  scoped_ptr<GDataDirectory> root_;
  base::Lock lock_;

  // The profile hosts the GDataFileSystem.
  Profile* profile_;

  // The document service for the GDataFileSystem.
  scoped_ptr<DocumentsService> documents_service_;

  base::WeakPtrFactory<GDataFileSystem> weak_ptr_factory_;
  base::WeakPtr<GDataFileSystem> file_system_bound_to_ui_thread_;
  base::WeakPtr<DocumentsService> documents_service_bound_to_ui_thread_;
};

// Singleton that owns all GDataFileSystems and associates them with
// Profiles.
class GDataFileSystemFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the GDataFileSystem for |profile|, creating it if it is not
  // yet created.
  static GDataFileSystem* GetForProfile(Profile* profile);

  // Returns the GDataFileSystemFactory instance.
  static GDataFileSystemFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<GDataFileSystemFactory>;

  GDataFileSystemFactory();
  virtual ~GDataFileSystemFactory();

  // ProfileKeyedServiceFactory:
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_FILE_SYSTEM_H_
