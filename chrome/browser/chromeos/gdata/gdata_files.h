// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_FILES_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_FILES_H_
#pragma once

#include <map>
#include <string>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/platform_file.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/chromeos/gdata/gdata_params.h"
#include "chrome/browser/chromeos/gdata/gdata_parser.h"
#include "chrome/browser/chromeos/gdata/gdata_uploader.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace gdata {

class GDataDirectory;
class GDataFile;
class GDataFileSystem;
class GDataRootDirectory;

// Callback for GetCacheState operation.
typedef base::Callback<void(base::PlatformFileError error,
                            int cache_state)> GetCacheStateCallback;

// Directory content origin.
enum ContentOrigin {
  UNINITIALIZED,
  // Directory content is currently loading from somewhere.  needs to wait.
  INITIALIZING,
  // Directory content is initialized, but during refreshing.
  REFRESHING,
  // Directory content is initialized from disk cache.
  FROM_CACHE,
  // Directory content is initialized from the direct server response.
  FROM_SERVER,
};

// File type on the gdata file system can be either a regular file or
// a hosted document.
enum GDataFileType {
  REGULAR_FILE,
  HOSTED_DOCUMENT,
};

// Base class for representing files and directories in gdata virtual file
// system.
class GDataFileBase {
 public:
  explicit GDataFileBase(GDataDirectory* parent, GDataRootDirectory* root);
  virtual ~GDataFileBase();
  virtual GDataFile* AsGDataFile();
  virtual GDataDirectory* AsGDataDirectory();
  virtual GDataRootDirectory* AsGDataRootDirectory();

  // Converts DocumentEntry into GDataFileBase.
  static GDataFileBase* FromDocumentEntry(GDataDirectory* parent,
                                          DocumentEntry* doc,
                                          GDataRootDirectory* root);

  // Escapes forward slashes from file names with magic unicode character
  // \u2215 pretty much looks the same in UI.
  static std::string EscapeUtf8FileName(const std::string& input);

  // Unescapes what was escaped in EScapeUtf8FileName.
  static std::string UnescapeUtf8FileName(const std::string& input);

  GDataDirectory* parent() { return parent_; }
  const base::PlatformFileInfo& file_info() const { return file_info_; }
  const FilePath::StringType& file_name() const { return file_name_; }
  const FilePath::StringType& title() const {
    return title_;
  }
  void set_title(const FilePath::StringType& title) {
    title_ = title;
  }
  void set_file_name(const FilePath::StringType& name) { file_name_ = name; }

  // The unique resource ID associated with this file system entry.
  const std::string& resource_id() const { return resource_id_; }

  // The content URL is used for downloading regular files as is.
  const GURL& content_url() const { return content_url_; }

  // The self URL is used for removing files and hosted documents.
  const GURL& self_url() const { return self_url_; }

  // Returns virtual file path representing this file system entry. This path
  // corresponds to file path expected by public methods of GDataFileSyste
  // class.
  FilePath GetFilePath();

  // Sets |file_name_| based on the value of |title_| without name
  // de-duplication (see AddFile() for details on de-duplication).
  virtual void SetFileNameFromTitle();

 protected:
  // GDataDirectory::TakeFile() needs to call GDataFileBase::set_parent().
  friend class GDataDirectory;

  // Sets the parent directory of this file system entry.
  // It is intended to be used by GDataDirectory::AddFile() only.
  void set_parent(GDataDirectory* parent) { parent_ = parent; }

  base::PlatformFileInfo file_info_;
  // Name of this file in the gdata virtual file system.
  FilePath::StringType file_name_;
  // Title of this file (i.e. the 'title' attribute associated with a regular
  // file, hosted document, or collection). The title is used to derive
  // |file_name_| but may be different from |file_name_|. For example,
  // |file_name_| has an added .g<something> extension for hosted documents or
  // may have an extra suffix for name de-duplication on the gdata file system.
  FilePath::StringType title_;
  std::string resource_id_;
  // Files with the same title will be uniquely identified with this field
  // so we can represent them with unique URLs/paths in File API layer.
  // For example, two files in the same directory with the same name "Foo"
  // will show up in the virtual directory as "Foo" and "Foo (2)".
  GURL self_url_;
  GURL content_url_;
  GDataDirectory* parent_;
  GDataRootDirectory* root_;  // Weak pointer to GDataRootDirectory.

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

  explicit GDataFile(GDataDirectory* parent, GDataRootDirectory* root);
  virtual ~GDataFile();
  virtual GDataFile* AsGDataFile() OVERRIDE;

  static GDataFileBase* FromDocumentEntry(GDataDirectory* parent,
                                          DocumentEntry* doc,
                                          GDataRootDirectory* root);

  static bool IsCachePresent(int cache_state) {
    return cache_state & CACHE_STATE_PRESENT;
  }
  static bool IsCachePinned(int cache_state) {
    return cache_state & CACHE_STATE_PINNED;
  }
  static bool IsCacheDirty(int cache_state) {
    return cache_state & CACHE_STATE_DIRTY;
  }
  static int SetCachePresent(int cache_state) {
    return cache_state |= CACHE_STATE_PRESENT;
  }
  static int SetCachePinned(int cache_state) {
    return cache_state |= CACHE_STATE_PINNED;
  }
  static int SetCacheDirty(int cache_state) {
    return cache_state |= CACHE_STATE_DIRTY;
  }
  static int ClearCachePresent(int cache_state) {
    return cache_state &= ~CACHE_STATE_PRESENT;
  }
  static int ClearCachePinned(int cache_state) {
    return cache_state &= ~CACHE_STATE_PINNED;
  }
  static int ClearCacheDirty(int cache_state) {
    return cache_state &= ~CACHE_STATE_DIRTY;
  }

  DocumentEntry::EntryKind kind() const { return kind_; }
  const GURL& thumbnail_url() const { return thumbnail_url_; }
  const GURL& edit_url() const { return edit_url_; }
  const std::string& content_mime_type() const { return content_mime_type_; }
  const std::string& etag() const { return etag_; }
  const std::string& id() const { return id_; }
  const std::string& file_md5() const { return file_md5_; }
  // The |callback| is invoked with a bitmask of CacheState enum values.
  void GetCacheState(const GetCacheStateCallback& callback);
  const std::string& document_extension() const { return document_extension_; }
  bool is_hosted_document() const { return is_hosted_document_; }

  // Overrides GDataFileBase::SetFileNameFromTitle() to set |file_name_| based
  // on the value of |title_| as well as |is_hosted_document_| and
  // |document_extension_| for hosted documents.
  virtual void SetFileNameFromTitle() OVERRIDE;

 private:
  // Content URL for files.
  DocumentEntry::EntryKind kind_;
  GURL thumbnail_url_;
  GURL edit_url_;
  std::string content_mime_type_;
  std::string etag_;
  std::string id_;
  std::string file_md5_;
  std::string document_extension_;
  bool is_hosted_document_;

  DISALLOW_COPY_AND_ASSIGN(GDataFile);
};

// Represents "directory" in a GData virtual file system. Maps to gdata
// collection element.
class GDataDirectory : public GDataFileBase {
 public:
  GDataDirectory(GDataDirectory* parent, GDataRootDirectory* root);
  virtual ~GDataDirectory();
  virtual GDataDirectory* AsGDataDirectory() OVERRIDE;

  static GDataFileBase* FromDocumentEntry(GDataDirectory* parent,
                                          DocumentEntry* doc,
                                          GDataRootDirectory* root);

  // Adds child file to the directory and takes over the ownership of |file|
  // object. The method will also do name de-duplication to ensure that the
  // exposed presentation path does not have naming conflicts. Two files with
  // the same name "Foo" will be renames to "Foo (1)" and "Foo (2)".
  void AddFile(GDataFileBase* file);

  // Takes the ownership of |file| from its current parent. If this directory
  // is already the current parent of |file|, this method effectively goes
  // through the name de-duplication for |file| based on the current state of
  // the file system.
  bool TakeFile(GDataFileBase* file);

  // Removes the file from its children list and destroys the file instance.
  bool RemoveFile(GDataFileBase* file);

  // Removes children elements.
  void RemoveChildren();

  // Checks if directory content needs to be refreshed from the server.
  bool NeedsRefresh() const;

  // Last refresh time.
  const base::Time& refresh_time() const { return refresh_time_; }
  void set_refresh_time(const base::Time& time) { refresh_time_ = time; }
  // Url for this feed.
  const GURL& start_feed_url() const { return start_feed_url_; }
  void set_start_feed_url(const GURL& url) { start_feed_url_ = url; }
  // Continuing feed's url.
  const GURL& next_feed_url() const { return next_feed_url_; }
  void set_next_feed_url(const GURL& url) { next_feed_url_ = url; }
  // Upload url is an entry point for initialization of file upload.
  // It corresponds to resumable-create-media link from gdata feed.
  const GURL& upload_url() const { return upload_url_; }
  void set_upload_url(const GURL& url) { upload_url_ = url; }
  // Collection of children GDataFileBase items.
  const GDataFileCollection& children() const { return children_; }
  // Directory content origin.
  const ContentOrigin origin() const { return origin_; }
  void set_origin(ContentOrigin value) { origin_ = value; }

 private:
  // Removes the file from its children list without destroying the
  // file instance.
  bool RemoveFileFromChildrenList(GDataFileBase* file);

  base::Time refresh_time_;
  // Url for this feed.
  GURL start_feed_url_;
  // Continuing feed's url.
  GURL next_feed_url_;
  // Upload url, corresponds to resumable-create-media link for feed
  // representing this directory.
  GURL upload_url_;
  // Collection of children GDataFileBase items.
  GDataFileCollection children_;
  // Directory content origin.
  ContentOrigin origin_;

  DISALLOW_COPY_AND_ASSIGN(GDataDirectory);
};

class GDataRootDirectory : public GDataDirectory {
 public:
  // Enum defining GCache subdirectory location.
  // This indexes into |GDataFileSystem::cache_paths_| vector.
  enum CacheSubDirectoryType {
    CACHE_TYPE_META = 0,       // Downloaded feeds.
    CACHE_TYPE_PINNED,         // Symlinks to files in persistent dir that are
                               // pinned, or to /dev/null for non-existent
                               // files.
    CACHE_TYPE_OUTGOING,       // Symlinks to files in persistent or tmp dir to
                               // be uploaded.
    CACHE_TYPE_PERSISTENT,     // Files that are pinned or modified locally,
                               // not evictable, hopefully.
    CACHE_TYPE_TMP,            // Files that don't meet criteria to be in
                               // persistent dir, and hence evictable.
    CACHE_TYPE_TMP_DOWNLOADS,  // Downloaded files.
    CACHE_TYPE_TMP_DOCUMENTS,  // Temporary JSON files for hosted documents.
  };

  // Structure to store information of an existing cache file.
  struct CacheEntry {
    CacheEntry(const std::string& in_md5,
               CacheSubDirectoryType in_sub_dir_type,
               int in_cache_state)
        : md5(in_md5),
          sub_dir_type(in_sub_dir_type),
          cache_state(in_cache_state) {
    }

    bool IsPresent() const {
      return GDataFile::IsCachePresent(cache_state);
    }
    bool IsPinned() const {
      return GDataFile::IsCachePinned(cache_state);
    }
    bool IsDirty() const {
      return GDataFile::IsCacheDirty(cache_state);
    }

    // For debugging purposes.
    std::string ToString() const;

    std::string md5;
    CacheSubDirectoryType sub_dir_type;
    int cache_state;
  };

  // A map table of cache file's resource id to its CacheEntry* entry.
  typedef std::map<std::string, CacheEntry*> CacheMap;

  // A map table of file's resource string to its GDataFile* entry.
  typedef std::map<std::string, GDataFileBase*> ResourceMap;

  explicit GDataRootDirectory(GDataFileSystem* file_system);
  virtual ~GDataRootDirectory();

  // GDataFileBase implementation.

  virtual GDataRootDirectory* AsGDataRootDirectory() OVERRIDE;

  // Add file to resource map.
  void AddFileToResourceMap(GDataFileBase* file);

  // Remove file from resource map.
  void RemoveFileFromResourceMap(GDataFileBase* file);

  // Remove files from resource map.
  void RemoveFilesFromResourceMap(const GDataFileCollection& children);

  // Returns the GDataFileBase* with the corresponding |resource_id|.
  GDataFileBase* GetFileByResourceId(const std::string& resource_id);

  // Set |cache_map_| data member to formal parameter |new_cache_map|.
  void SetCacheMap(const CacheMap& new_cache_map);

  // Updates cache map with entry corresponding to |resource_id|.
  // Creates new entry if it doesn't exist, otherwise update the entry.
  void UpdateCacheMap(const std::string& resource_id,
                      const std::string& md5,
                      CacheSubDirectoryType subdir,
                      int cache_state);

  // Removes entry corresponding to |resource_id| from cache map.
  void RemoveFromCacheMap(const std::string& resource_id);

  // Returns the cache entry for file corresponding to |resource_id| and |md5|
  // if entry exists in cache map.  Otherwise, returns NULL.
  // |md5| can be empty if only matching |resource_id| is desired, which may
  // happen when looking for pinned entries where symlinks' filenames have no
  // extension and hence no md5.
  CacheEntry* GetCacheEntry(const std::string& resource_id,
                            const std::string& md5);

  // Gets the state of the cache file corresponding to |resource_id| and |md5|
  // asynchronously where |callback| will be invoked with the cache state.
  void GetCacheState(const std::string& resource_id,
                     const std::string& md5,
                     const GetCacheStateCallback& callback);

 private:
  ResourceMap resource_map_;
  CacheMap cache_map_;

  // Weak pointer to GDataFileSystem that owns us.
  GDataFileSystem* file_system_;

  DISALLOW_COPY_AND_ASSIGN(GDataRootDirectory);
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_FILES_H_
