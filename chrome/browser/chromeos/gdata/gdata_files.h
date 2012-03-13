// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_FILES_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_FILES_H_

#include <sys/stat.h>

#include <map>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/platform_file.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/chromeos/gdata/gdata.h"
#include "chrome/browser/chromeos/gdata/gdata_parser.h"
#include "chrome/browser/chromeos/gdata/gdata_uploader.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace gdata {

class GDataDirectory;
class GDataFile;
class GDataRootDirectory;

// Base class for representing files and directories in gdata virtual file
// system.
class GDataFileBase {
 public:
  explicit GDataFileBase(GDataDirectory* parent);
  virtual ~GDataFileBase();
  virtual GDataFile* AsGDataFile();
  virtual GDataDirectory* AsGDataDirectory();
  virtual GDataRootDirectory* AsGDataRootDirectory();

  // Converts DocumentEntry into GDataFileBase.
  static GDataFileBase* FromDocumentEntry(GDataDirectory* parent,
                                          DocumentEntry* doc);

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
  // It is intended to be used by GDataDirectory::TakeFile() only.
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
  const std::string& id() const { return id_; }
  const std::string& file_md5() const { return file_md5_; }
  // Returns a bitmask of CacheState enum values.
  int GetCacheState();
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
  explicit GDataDirectory(GDataDirectory* parent);
  virtual ~GDataDirectory();
  virtual GDataDirectory* AsGDataDirectory() OVERRIDE;

  static GDataFileBase* FromDocumentEntry(GDataDirectory* parent,
                                          DocumentEntry* doc);

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

  // Checks if directory content needs to be retrieved again. If it does,
  // the function will return URL for next feed in |next_feed_url|.
  bool NeedsRefresh(GURL* next_feed_url);

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

  DISALLOW_COPY_AND_ASSIGN(GDataDirectory);
};

class GDataRootDirectory : public GDataDirectory {
 public:
  // Status flags of cache file, represented by the RWX (read, write,
  // executable) permissions of others category (since they're not being used).
  enum CacheStatusFlags {
    // Set read permission for others if file is not corrupted i.e. it was
    // closed properly after previous operations.
    CACHE_OK = S_IROTH,

    // Set write permission for others if file is dirty and needs to be uploaded
    // to gdata.
    CACHE_DIRTY = S_IWOTH,

    // Set executable permission for others if file is pinned.
    CACHE_PINNED = S_IXOTH,

    // Indicates that file is ok and dirty and needs to be uploaded.
    CACHE_UPDATED = CACHE_OK | CACHE_DIRTY,
  };

  // Structure to store information of an existing cache file.
  struct CacheEntry {
    CacheEntry(const std::string& in_md5, mode_t in_mode_bits)
        : md5(in_md5),
          mode_bits(in_mode_bits) {
    }

    std::string md5;
    mode_t mode_bits;
  };

  // A map table of cache file's resource id to its CacheEntry* entry.
  typedef std::map<std::string, CacheEntry*> CacheMap;

  // A map table of file's resource string to its GDataFile* entry.
  typedef std::map<std::string, GDataFileBase*> ResourceMap;

  GDataRootDirectory();
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
  GDataFileBase* GetFileByResource(const std::string& resource_id);

  // Set |cache_map_| data member to formal parameter |new_cache_map|.
  void SetCacheMap(const CacheMap& new_cache_map);

  // Updates cache map with entry corresponding to |res_id|.
  // Returns false if cache has not been initialized.
  void UpdateCacheMap(const std::string& res_id,
                      const std::string& md5,
                      mode_t mode_bits);

  // Removes entry corresponding to |res_id| from cache map.
  void RemoveFromCacheMap(const std::string& res_id);

  // Checks if file corresponding to |res_id| and |md5| exists in cache map.
  bool CacheFileExists(const std::string& res_id,
                       const std::string& md5);

  // Gets the state of the cache file corresponding to |res_id| and |md5|.
  int GetCacheState(const std::string& res_id,
                    const std::string& md5);

 private:
  ResourceMap resource_map_;

  CacheMap cache_map_;

  DISALLOW_COPY_AND_ASSIGN(GDataRootDirectory);
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_FILES_H_
