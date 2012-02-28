// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_FILE_SYSTEM_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_FILE_SYSTEM_H_

#include <map>

#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/platform_file.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/chromeos/gdata/gdata.h"
#include "chrome/browser/chromeos/gdata/gdata_parser.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace gdata {

class GDataDirectory;
class GDataFile;

// Base class for representing files and directories in gdata virtual file
// system.
class GDataFileBase {
 public:
  GDataFileBase();
  virtual ~GDataFileBase();
  // Converts gdata::DocumentEntry into chromeos::GDataFileBase.
  static GDataFileBase* FromDocumentEntry(gdata::DocumentEntry* doc);
  virtual GDataFile* AsGDataFile();
  virtual GDataDirectory* AsGDataDirectory();
  const base::PlatformFileInfo& file_info() const { return file_info_; }
  const FilePath::StringType& file_name() const { return file_name_; }
  const FilePath::StringType& original_file_name() const {
    return original_file_name_;
  }
  void set_file_name(const FilePath::StringType& name) { file_name_ = name; }
  const GURL& self_url() const { return self_url_; }

 protected:
  base::PlatformFileInfo file_info_;
  FilePath::StringType file_name_;
  FilePath::StringType original_file_name_;
  // Files with the same original name will be uniquely identified with this
  // field so we can represent them with unique URLs/paths in File API layer.
  // For example, two files in the same directory with the same name "Foo"
  // will show up in the virtual directory as "Foo" and "Foo (2)".
  GURL self_url_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GDataFileBase);
};

typedef std::map<FilePath::StringType, GDataFileBase*> GDataFileCollection;

// Represents "file" in in a GData virtual file system. On gdata feed side,
// this could be either a regular file or a server side document.
class GDataFile : public GDataFileBase {
 public:
  GDataFile();
  virtual ~GDataFile();
  virtual GDataFile* AsGDataFile() OVERRIDE;

  static GDataFileBase* FromDocumentEntry(gdata::DocumentEntry* doc);

  gdata::DocumentEntry::EntryKind kind() const { return kind_; }
  const GURL& content_url() const { return content_url_; }
  const std::string& content_mime_type() const { return content_mime_type_; }
  const std::string& etag() const { return etag_; }
  const std::string& resource() const { return resource_id_; }
  const std::string& id() const { return id_; }
  const std::string& file_md5() const { return file_md5_; }

 private:
  // Content URL for files.
  gdata::DocumentEntry::EntryKind kind_;
  GURL content_url_;
  std::string content_mime_type_;
  std::string etag_;
  std::string resource_id_;
  std::string id_;
  std::string file_md5_;

  DISALLOW_COPY_AND_ASSIGN(GDataFile);
};

// Represents "directory" in a GData virtual file system. Maps to gdata
// collection element.
class GDataDirectory : public GDataFileBase {
 public:
  GDataDirectory();
  virtual ~GDataDirectory();
  virtual GDataDirectory* AsGDataDirectory() OVERRIDE;

  static GDataFileBase* FromDocumentEntry(gdata::DocumentEntry* doc);

  // Adds child file to the directory and takes over the ownership of |file|
  // object. The method will also do name deduplication to ensure that the
  // exposed presentation path does not have naming conflicts. Two files with
  // the same name "Foo" will be renames to "Foo (1)" and "Foo (2)".
  void AddFile(GDataFileBase* file);

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

// GData file system abstraction layer. This class is refcounted since we
// access it from different threads and aggregate into number of other objects.
// GDataFileSystem is per-profie, hence inheriting ProfileKeyedService.
class GDataFileSystem : public base::RefCountedThreadSafe<GDataFileSystem>,
                        public ProfileKeyedService {
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


  // ProfileKeyedService override:
  virtual void Shutdown() OVERRIDE;

  // Finds file info by using virtual |file_path|. If |require_content| is set,
  // the found directory will be pre-populated before passed back to the
  // |delegate|. If |allow_refresh| is not set, directories' content
  // won't be performed.
  void FindFileByPath(const FilePath& file_path,
                      scoped_refptr<FindFileDelegate> delegate);


  // Initiates directory feed fetching operation and continues previously
  // initiated FindFileByPath() attempt upon its completion. Safe to be called
  // from any thread. Internally, it will route content refresh request to
  // RefreshFeedOnUIThread() method which will initiated content fetching from
  // UI thread as required by gdata library (UrlFetcher).
  void StartDirectoryRefresh(const FindFileParams& params);

 private:
  friend class base::RefCountedThreadSafe<GDataFileSystem>;
  friend class GDataFileSystemFactory;
  friend class GDataFileSystemTest;

  explicit GDataFileSystem(Profile* profile);
  virtual ~GDataFileSystem();

  // Unsafe (unlocked) version of the function above.
  void UnsafeFindFileByPath(const FilePath& file_path,
                            scoped_refptr<FindFileDelegate> delegate);

  // Initiates document feed fetching from UI thread.
  void RefreshFeedOnUIThread(const GURL& feed_url,
                             const gdata::GetDataCallback& callback);

  // Converts document feed from gdata service into DirectoryInfo. On failure,
  // returns NULL and fills in |error| with an appropriate value.
  GDataDirectory* ParseGDataFeed(gdata::GDataErrorCode status,
                                 base::Value* data,
                                 base::PlatformFileError *error);

  // Callback for handling feed content fetching while searching for file info.
  // This callback is invoked after async feed fetch operation that was
  // invoked by StartDirectoryRefresh() completes. This callback will update
  // the content of the refreshed directory object and continue initially
  // started FindFileByPath() request.
  void OnGetDocuments(const FindFileParams& params,
                      gdata::GDataErrorCode status,
                      base::Value* data);

  // Updates content of the directory identified with |directory_path|. If the
  // feed was not complete, it will return URL for the remaining portion in
  // |next_feed|. On success, returns PLATFORM_FILE_OK.
  base::PlatformFileError UpdateDirectoryWithDocumentFeed(
      const FilePath& directory_path,
      const GURL& feed_url,
      base::Value* data,
      bool is_initial_feed,
      GURL* next_feed);

  scoped_ptr<GDataDirectory> root_;
  base::Lock lock_;

  // The profile hosts the GDataFileSystem.
  Profile* profile_;
};

// Singleton that owns all GDataFileSystems and associates them with
// Profiles.
class GDataFileSystemFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the GDataFileSystem for |profile|, creating it if it is not
  // yet created.
  static GDataFileSystem* GetForProfile(Profile* profile);

  // Returns the GDataFileSystemFactory instance.
  static GDataFileSystemFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<GDataFileSystemFactory>;

  GDataFileSystemFactory();
  virtual ~GDataFileSystemFactory();

  // ProfileKeyedServiceFactory:
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_FILE_SYSTEM_H_
