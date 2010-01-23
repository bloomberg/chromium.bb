// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_OS_EXCHANGE_DATA_H_
#define APP_OS_EXCHANGE_DATA_H_

#include "build/build_config.h"

#include <set>
#include <string>
#include <vector>

#if defined(OS_WIN)
#include <objidl.h>
#elif !defined(OS_MACOSX)
#include <gtk/gtk.h>
#endif

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"

class GURL;
class Pickle;

///////////////////////////////////////////////////////////////////////////////
//
// OSExchangeData
//  An object that holds interchange data to be sent out to OS services like
//  clipboard, drag and drop, etc. This object exposes an API that clients can
//  use to specify raw data and its high level type. This object takes care of
//  translating that into something the OS can understand.
//
///////////////////////////////////////////////////////////////////////////////

// NOTE: Support for html and file contents is required by TabContentViewWin.
// TabContentsViewGtk uses a different class to handle drag support that does
// not use OSExchangeData. As such, file contents and html support is only
// compiled on windows.
class OSExchangeData {
 public:
  // CustomFormats are used for non-standard data types. For example, bookmark
  // nodes are written using a CustomFormat.
#if defined(OS_WIN)
  typedef CLIPFORMAT CustomFormat;
#elif !defined(OS_MACOSX)
  typedef GdkAtom CustomFormat;
#endif

  // Enumeration of the known formats.
  enum Format {
    STRING         = 1 << 0,
    URL            = 1 << 1,
    FILE_NAME      = 1 << 2,
    PICKLED_DATA   = 1 << 3,
#if defined(OS_WIN)
    FILE_CONTENTS  = 1 << 4,
    HTML           = 1 << 5,
#endif
  };

  struct DownloadFileInfo;

  // Defines the interface to observe the status of file download.
  class DownloadFileObserver : public base::RefCounted<DownloadFileObserver> {
   public:
    // The caller is responsible to free the DownloadFileInfo objects passed
    // in the vector parameter.
    virtual void OnDataReady(
        int format,
        const std::vector<DownloadFileInfo*>& downloads) = 0;

   protected:
    friend class base::RefCounted<DownloadFileObserver>;
    virtual ~DownloadFileObserver() {}
  };

  // Defines the interface to control how a file is downloaded.
  class DownloadFileProvider :
      public base::RefCountedThreadSafe<DownloadFileProvider> {
   public:
    virtual bool Start(DownloadFileObserver* observer, int format) = 0;
    virtual void Stop() = 0;

   protected:
    friend class base::RefCountedThreadSafe<DownloadFileProvider>;
    virtual ~DownloadFileProvider() {}
  };

  // Encapsulates the info about a file to be downloaded.
  struct DownloadFileInfo {
    FilePath filename;
    uint64 size;
    scoped_refptr<DownloadFileProvider> downloader;

    DownloadFileInfo(const FilePath& filename,
                     uint64 size,
                     DownloadFileProvider* downloader)
        : filename(filename),
          size(size),
          downloader(downloader) {}
  };

  // Provider defines the platform specific part of OSExchangeData that
  // interacts with the native system.
  class Provider {
   public:
    Provider() {}
    virtual ~Provider() {}

    virtual void SetString(const std::wstring& data) = 0;
    virtual void SetURL(const GURL& url, const std::wstring& title) = 0;
    virtual void SetFilename(const std::wstring& full_path) = 0;
    virtual void SetPickledData(CustomFormat format, const Pickle& data) = 0;

    virtual bool GetString(std::wstring* data) const = 0;
    virtual bool GetURLAndTitle(GURL* url, std::wstring* title) const = 0;
    virtual bool GetFilename(std::wstring* full_path) const = 0;
    virtual bool GetPickledData(CustomFormat format, Pickle* data) const = 0;

    virtual bool HasString() const = 0;
    virtual bool HasURL() const = 0;
    virtual bool HasFile() const = 0;
    virtual bool HasCustomFormat(
        OSExchangeData::CustomFormat format) const = 0;

#if defined(OS_WIN)
    virtual void SetFileContents(const std::wstring& filename,
                                 const std::string& file_contents) = 0;
    virtual void SetHtml(const std::wstring& html, const GURL& base_url) = 0;
    virtual bool GetFileContents(std::wstring* filename,
                                 std::string* file_contents) const = 0;
    virtual bool GetHtml(std::wstring* html, GURL* base_url) const = 0;
    virtual bool HasFileContents() const = 0;
    virtual bool HasHtml() const = 0;
    virtual void SetDownloadFileInfo(DownloadFileInfo* download) = 0;
#endif
  };

  OSExchangeData();
  // Creates an OSExchangeData with the specified provider. OSExchangeData
  // takes ownership of the supplied provider.
  explicit OSExchangeData(Provider* provider);

  ~OSExchangeData();

  // Registers the specific string as a possible format for data.
  static CustomFormat RegisterCustomFormat(const std::string& type);

  // Returns the Provider, which actually stores and manages the data.
  const Provider& provider() const { return *provider_; }
  Provider& provider() { return *provider_; }

  // These functions add data to the OSExchangeData object of various Chrome
  // types. The OSExchangeData object takes care of translating the data into
  // a format suitable for exchange with the OS.
  // NOTE WELL: Typically, a data object like this will contain only one of the
  //            following types of data. In cases where more data is held, the
  //            order in which these functions are called is _important_!
  //       ---> The order types are added to an OSExchangeData object controls
  //            the order of enumeration in our IEnumFORMATETC implementation!
  //            This comes into play when selecting the best (most preferable)
  //            data type for insertion into a DropTarget.
  void SetString(const std::wstring& data);
  // A URL can have an optional title in some exchange formats.
  void SetURL(const GURL& url, const std::wstring& title);
  // A full path to a file.
  // TODO: convert to Filepath.
  void SetFilename(const std::wstring& full_path);
  // Adds pickled data of the specified format.
  void SetPickledData(CustomFormat format, const Pickle& data);

  // These functions retrieve data of the specified type. If data exists, the
  // functions return and the result is in the out parameter. If the data does
  // not exist, the out parameter is not touched. The out parameter cannot be
  // NULL.
  bool GetString(std::wstring* data) const;
  bool GetURLAndTitle(GURL* url, std::wstring* title) const;
  // Return the path of a file, if available.
  bool GetFilename(std::wstring* full_path) const;
  bool GetPickledData(CustomFormat format, Pickle* data) const;

  // Test whether or not data of certain types is present, without actually
  // returning anything.
  bool HasString() const;
  bool HasURL() const;
  bool HasFile() const;
  bool HasCustomFormat(CustomFormat format) const;

  // Returns true if this OSExchangeData has data for ALL the formats in
  // |formats| and ALL the custom formats in |custom_formats|.
  bool HasAllFormats(int formats,
                     const std::set<CustomFormat>& custom_formats) const;

  // Returns true if this OSExchangeData has data in any of the formats in
  // |formats| or any custom format in |custom_formats|.
  bool HasAnyFormat(int formats,
                     const std::set<CustomFormat>& custom_formats) const;

#if defined(OS_WIN)
  // Adds the bytes of a file (CFSTR_FILECONTENTS and CFSTR_FILEDESCRIPTOR).
  void SetFileContents(const std::wstring& filename,
                       const std::string& file_contents);
  // Adds a snippet of HTML.  |html| is just raw html but this sets both
  // text/html and CF_HTML.
  void SetHtml(const std::wstring& html, const GURL& base_url);
  bool GetFileContents(std::wstring* filename,
                       std::string* file_contents) const;
  bool GetHtml(std::wstring* html, GURL* base_url) const;

  // Adds a download file with full path (CF_HDROP).
  void SetDownloadFileInfo(DownloadFileInfo* download);
#endif

 private:
  // Creates the platform specific Provider.
  static Provider* CreateProvider();

  // Provides the actual data.
  scoped_ptr<Provider> provider_;

  DISALLOW_COPY_AND_ASSIGN(OSExchangeData);
};

#endif  // APP_OS_EXCHANGE_DATA_H_
