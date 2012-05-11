// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_FILES_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_FILES_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/platform_file.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/chromeos/gdata/find_entry_callback.h"
#include "chrome/browser/chromeos/gdata/gdata_params.h"
#include "chrome/browser/chromeos/gdata/gdata_parser.h"
#include "chrome/browser/chromeos/gdata/gdata_uploader.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace gdata {

class GDataFile;
class GDataDirectory;
class GDataRootDirectory;

class GDataEntryProto;
class GDataFileProto;
class GDataDirectoryProto;
class GDataRootDirectoryProto;
class PlatformFileInfoProto;

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
class GDataEntry {
 public:
  explicit GDataEntry(GDataDirectory* parent, GDataRootDirectory* root);
  virtual ~GDataEntry();

  virtual GDataFile* AsGDataFile();
  virtual GDataDirectory* AsGDataDirectory();
  virtual GDataRootDirectory* AsGDataRootDirectory();

  // const versions of AsGDataFile and AsGDataDirectory.
  const GDataFile* AsGDataFileConst() const;
  const GDataDirectory* AsGDataDirectoryConst() const;

  // Converts DocumentEntry into GDataEntry.
  static GDataEntry* FromDocumentEntry(GDataDirectory* parent,
                                       DocumentEntry* doc,
                                       GDataRootDirectory* root);

  // Serialize/Parse to/from string via proto classes.
  // TODO(achuith): Correctly set up parent_ and root_ links in
  // FromProtoString.
  void SerializeToString(std::string* serialized_proto) const;
  static scoped_ptr<GDataEntry> FromProtoString(
      const std::string& serialized_proto);

  // Converts the proto representation to the platform file.
  static void ConvertProtoToPlatformFileInfo(
      const PlatformFileInfoProto& proto,
      base::PlatformFileInfo* file_info);

  // Converts the platform file info to the proto representation.
  static void ConvertPlatformFileInfoToProto(
      const base::PlatformFileInfo& file_info,
      PlatformFileInfoProto* proto);

  // Converts to/from proto.
  void FromProto(const GDataEntryProto& proto);
  void ToProto(GDataEntryProto* proto) const;

  // Escapes forward slashes from file names with magic unicode character
  // \u2215 pretty much looks the same in UI.
  static std::string EscapeUtf8FileName(const std::string& input);

  // Unescapes what was escaped in EScapeUtf8FileName.
  static std::string UnescapeUtf8FileName(const std::string& input);

  // Return the parent of this entry. NULL for root.
  GDataDirectory* parent() const { return parent_; }
  const base::PlatformFileInfo& file_info() const { return file_info_; }

  // This is not the full path, use GetFilePath for that.
  // Note that file_name_ gets reset by SetFileNameFromTitle() in a number of
  // situations due to de-duplication (see AddEntry).
  // TODO(achuith/satorux): Rename this to base_name.
  const FilePath::StringType& file_name() const { return file_name_; }
  // TODO(achuith): Make this private when GDataDB no longer uses path as a key.
  void set_file_name(const FilePath::StringType& name) { file_name_ = name; }

  const FilePath::StringType& title() const { return title_; }
  void set_title(const FilePath::StringType& title) { title_ = title; }

  // The unique resource ID associated with this file system entry.
  const std::string& resource_id() const { return resource_id_; }
  void set_resource_id(const std::string& res_id) { resource_id_ = res_id; }

  // The content URL is used for downloading regular files as is.
  const GURL& content_url() const { return content_url_; }
  void set_content_url(const GURL& url) { content_url_ = url; }

  // The edit URL is used for removing files and hosted documents.
  const GURL& edit_url() const { return edit_url_; }

  // The resource id of the parent folder. This piece of information is needed
  // to pair files from change feeds with their directory parents withing the
  // existing file system snapshot (GDataRootDirectory::resource_map_).
  const std::string& parent_resource_id() const { return parent_resource_id_; }

  // True if file was deleted. Used only for instances that are generated from
  // delta feeds.
  bool is_deleted() const { return deleted_; }

  // True if the entry is not bound to any file system (i.e. doesn't have a root
  // directory set). E.g. |fake_search_directory| below.
  // NOTE: GDataRootDirectories will return true here, since they have
  // themselves as root directories.
  bool is_detached() const { return root_ == NULL; }

  // Returns virtual file path representing this file system entry. This path
  // corresponds to file path expected by public methods of GDataFileSyste
  // class.
  FilePath GetFilePath() const;

  // Sets |file_name_| based on the value of |title_| without name
  // de-duplication (see AddEntry() for details on de-duplication).
  virtual void SetFileNameFromTitle();

 protected:
  // For access to SetParent from AddEntry.
  friend class GDataDirectory;

  // Sets the parent directory of this file system entry.
  // It is intended to be used by GDataDirectory::AddEntry() only.
  void SetParent(GDataDirectory* parent);

  base::PlatformFileInfo file_info_;
  // Title of this file (i.e. the 'title' attribute associated with a regular
  // file, hosted document, or collection). The title is used to derive
  // |file_name_| but may be different from |file_name_|. For example,
  // |file_name_| has an added .g<something> extension for hosted documents or
  // may have an extra suffix for name de-duplication on the gdata file system.
  FilePath::StringType title_;
  std::string resource_id_;
  std::string parent_resource_id_;
  // Files with the same title will be uniquely identified with this field
  // so we can represent them with unique URLs/paths in File API layer.
  // For example, two files in the same directory with the same name "Foo"
  // will show up in the virtual directory as "Foo" and "Foo (2)".
  GURL edit_url_;
  GURL content_url_;

  // Remaining fields are not serialized.

  // Name of this file in the gdata virtual file system. This can change
  // due to de-duplication (See AddEntry).
  FilePath::StringType file_name_;

  GDataDirectory* parent_;
  GDataRootDirectory* root_;  // Weak pointer to GDataRootDirectory.
  bool deleted_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GDataEntry);
};

typedef std::map<FilePath::StringType, GDataFile*> GDataFileCollection;
typedef std::map<FilePath::StringType, GDataDirectory*>
    GDataDirectoryCollection;

// Represents "file" in in a GData virtual file system. On gdata feed side,
// this could be either a regular file or a server side document.
class GDataFile : public GDataEntry {
 public:
  // This is used as a bitmask for the cache state.
  enum CacheState {
    CACHE_STATE_NONE    = 0x0,
    CACHE_STATE_PINNED  = 0x1 << 0,
    CACHE_STATE_PRESENT = 0x1 << 1,
    CACHE_STATE_DIRTY   = 0x1 << 2,
    CACHE_STATE_MOUNTED = 0x1 << 3,
  };

  explicit GDataFile(GDataDirectory* parent, GDataRootDirectory* root);
  virtual ~GDataFile();
  virtual GDataFile* AsGDataFile() OVERRIDE;

  // Converts DocumentEntry into GDataEntry.
  static GDataEntry* FromDocumentEntry(GDataDirectory* parent,
                                       DocumentEntry* doc,
                                       GDataRootDirectory* root);

  // Converts to/from proto.
  void FromProto(const GDataFileProto& proto);
  void ToProto(GDataFileProto* proto) const;

  static bool IsCachePresent(int cache_state) {
    return cache_state & CACHE_STATE_PRESENT;
  }
  static bool IsCachePinned(int cache_state) {
    return cache_state & CACHE_STATE_PINNED;
  }
  static bool IsCacheDirty(int cache_state) {
    return cache_state & CACHE_STATE_DIRTY;
  }
  static bool IsCacheMounted(int cache_state) {
    return cache_state & CACHE_STATE_MOUNTED;
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
  static int SetCacheMounted(int cache_state) {
    return cache_state |= CACHE_STATE_MOUNTED;
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
  static int ClearCacheMounted(int cache_state) {
    return cache_state &= ~CACHE_STATE_MOUNTED;
  }

  DocumentEntry::EntryKind kind() const { return kind_; }
  const GURL& thumbnail_url() const { return thumbnail_url_; }
  const GURL& alternate_url() const { return alternate_url_; }
  const std::string& content_mime_type() const { return content_mime_type_; }
  const std::string& etag() const { return etag_; }
  const std::string& id() const { return id_; }
  const std::string& file_md5() const { return file_md5_; }
  void set_file_md5(const std::string& file_md5) { file_md5_ = file_md5; }
  const std::string& document_extension() const { return document_extension_; }
  bool is_hosted_document() const { return is_hosted_document_; }

  // Overrides GDataEntry::SetFileNameFromTitle() to set |file_name_| based
  // on the value of |title_| as well as |is_hosted_document_| and
  // |document_extension_| for hosted documents.
  virtual void SetFileNameFromTitle() OVERRIDE;

 private:
  // Content URL for files.
  DocumentEntry::EntryKind kind_;
  GURL thumbnail_url_;
  GURL alternate_url_;
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
class GDataDirectory : public GDataEntry {
 public:
  GDataDirectory(GDataDirectory* parent, GDataRootDirectory* root);
  virtual ~GDataDirectory();
  virtual GDataDirectory* AsGDataDirectory() OVERRIDE;

  // Converts DocumentEntry into GDataEntry.
  static GDataEntry* FromDocumentEntry(GDataDirectory* parent,
                                       DocumentEntry* doc,
                                       GDataRootDirectory* root);

  // Converts to/from proto.
  void FromProto(const GDataDirectoryProto& proto);
  void ToProto(GDataDirectoryProto* proto) const;

  // Adds child file to the directory and takes over the ownership of |file|
  // object. The method will also do name de-duplication to ensure that the
  // exposed presentation path does not have naming conflicts. Two files with
  // the same name "Foo" will be renames to "Foo (1)" and "Foo (2)".
  void AddEntry(GDataEntry* entry);

  // Takes the ownership of |entry| from its current parent. If this directory
  // is already the current parent of |file|, this method effectively goes
  // through the name de-duplication for |file| based on the current state of
  // the file system.
  bool TakeEntry(GDataEntry* entry);

  // Takes over all entries from |dir|.
  bool TakeOverEntries(GDataDirectory* dir);

  // Find a child by its name.
  GDataEntry* FindChild(const FilePath::StringType& file_name) const;

  // Removes the entry from its children list and destroys the entry instance.
  bool RemoveEntry(GDataEntry* entry);

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
  // Upload url is an entry point for initialization of file upload.
  // It corresponds to resumable-create-media link from gdata feed.
  const GURL& upload_url() const { return upload_url_; }
  void set_upload_url(const GURL& url) { upload_url_ = url; }
  // Collection of children files/directories.
  const GDataFileCollection& child_files() const { return child_files_; }
  const GDataDirectoryCollection& child_directories() const {
    return child_directories_;
  }
  // Directory content origin.
  const ContentOrigin origin() const { return origin_; }
  void set_origin(ContentOrigin value) { origin_ = value; }

 private:
  // Add |entry| to children.
  void AddChild(GDataEntry* entry);

  // Removes the entry from its children without destroying the
  // entry instance.
  bool RemoveChild(GDataEntry* entry);

  base::Time refresh_time_;
  // Url for this feed.
  GURL start_feed_url_;
  // Continuing feed's url.
  GURL next_feed_url_;
  // Upload url, corresponds to resumable-create-media link for feed
  // representing this directory.
  GURL upload_url_;

  // Directory content origin.
  ContentOrigin origin_;

  // Collection of children GDataEntry items.
  GDataFileCollection child_files_;
  GDataDirectoryCollection child_directories_;

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
    NUM_CACHE_TYPES,           // This must be at the end.
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
    bool IsMounted() const {
      return GDataFile::IsCacheMounted(cache_state);
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
  typedef std::map<std::string, GDataEntry*> ResourceMap;

  GDataRootDirectory();
  virtual ~GDataRootDirectory();

  // Largest change timestamp that was the source of content for the current
  // state of the root directory.
  int largest_changestamp() const { return largest_changestamp_; }
  void set_largest_changestamp(int value) { largest_changestamp_ = value; }
  // Last time when we dumped serialized file system to disk.
  const base::Time& last_serialized() const { return last_serialized_; }
  void set_last_serialized(const base::Time& time) { last_serialized_ = time; }
  // Size of serialized file system on disk in bytes.
  const size_t serialized_size() const { return serialized_size_; }
  void set_serialized_size(size_t size) { serialized_size_ = size; }

  // GDataEntry implementation.
  virtual GDataRootDirectory* AsGDataRootDirectory() OVERRIDE;

  // Adds the entry to resource map.
  void AddEntryToResourceMap(GDataEntry* entry);

  // Removes the entry from resource map.
  void RemoveEntryFromResourceMap(GDataEntry* entry);

  // Searches for |file_path| triggering callback.
  void FindEntryByPath(const FilePath& file_path,
                       const FindEntryCallback& callback);

  // Returns the GDataEntry* with the corresponding |resource_id|.
  GDataEntry* GetEntryByResourceId(const std::string& resource_id);

  // Sets |cache_map_| data member to formal parameter |new_cache_map|.
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

  // Removes temporary files (files in CACHE_TYPE_TMP) from the cache map.
  void RemoveTemporaryFilesFromCacheMap();

  // Serializes/Parses to/from string via proto classes.
  void SerializeToString(std::string* serialized_proto) const;
  bool ParseFromString(const std::string& serialized_proto);

  // Converts to/from proto.
  void FromProto(const GDataRootDirectoryProto& proto);
  void ToProto(GDataRootDirectoryProto* proto) const;

 private:
  // Used in |FindEntryByPath| if the path that is being searched for is
  // pointing to a search result path. The find entry parameters should be
  // modified to point to the actual file system entry that is referenced by
  // virtual search path.
  // Search path is formatted: <search_result_path><search_result_child_path>.
  // <search_result_child_path> is used when search result is directory, and is
  // relative to search result path (id references some content inside search
  // result).
  // Search result file name will be formatted <resource_id>.<file_name>.
  // We can define "search result path references gdata entry" for gdata search
  // results by:
  // Entry that whose file name is <file_name>, and has the same parent as
  // the entry with resource id <resource_id>. This definition enables us to
  // test uniqueness of the proposed name when renaming gdata search result.
  //
  // For example, if drive/.search/foo/res_id.foo_name references
  // drive/result_parent/result, and the search path is
  // drive/.search/foo/res_ud.foo_name/foo_child, we'll set current dir to the
  // entry with path reulst_parent, and components to [result_parent, result,
  // foo_child].
  bool ModifyFindEntryParamsForSearchPath(
      const FilePath& file_path,
      std::vector<FilePath::StringType>* components,
      GDataDirectory** current_dir,
      FilePath* directory_path);

  ResourceMap resource_map_;
  CacheMap cache_map_;

  // Fake directory that will be returned when searching for content search
  // paths to make file manager happy when resolving paths. This directory
  // should never be used for file operations or storing file entries.
  scoped_ptr<GDataDirectory> fake_search_directory_;

  base::Time last_serialized_;
  int largest_changestamp_;
  size_t serialized_size_;

  DISALLOW_COPY_AND_ASSIGN(GDataRootDirectory);
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_FILES_H_
