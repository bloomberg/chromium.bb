// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_MEDIA_GALLERIES_HANDLER_H_
#define CHROME_BROWSER_EXTENSIONS_MEDIA_GALLERIES_HANDLER_H_

#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/manifest_handler.h"

class URLPattern;

// Encapsulates the state of a media gallery handler.
class MediaGalleriesHandler {
 public:
  typedef std::vector<linked_ptr<MediaGalleriesHandler> > List;

  MediaGalleriesHandler();
  ~MediaGalleriesHandler();

  std::string extension_id() const { return extension_id_; }
  void set_extension_id(const std::string& extension_id) {
    extension_id_ = extension_id;
  }

  const std::string& id() const { return id_; }
  void set_id(const std::string& id) { id_ = id; }

  // Default title.
  const std::string& title() const { return title_; }
  void set_title(const std::string& title) { title_ = title; }

  // Action icon path.
  const std::string icon_path() const { return default_icon_path_; }
  void set_icon_path(const std::string& path) {
    default_icon_path_ = path;
  }

  // Returns the media galleries handlers associated with the |extension|.
  static List* GetHandlers(const extensions::Extension* extension);

 private:
  // The id for the extension this action belongs to (as defined in the
  // extension manifest).
  std::string extension_id_;
  std::string title_;
  std::string default_icon_path_;
  // The id for the MediaGalleriesHandler, for example: "ImportToDrive".
  std::string id_;
};

// Parses the "media_galleries_handlers" extension manifest key.
class MediaGalleriesHandlerParser : public extensions::ManifestHandler {
 public:
  MediaGalleriesHandlerParser();
  virtual ~MediaGalleriesHandlerParser();

  virtual bool Parse(extensions::Extension* extension,
                     string16* error) OVERRIDE;

 private:
  virtual const std::vector<std::string> Keys() const OVERRIDE;
};

#endif  // CHROME_BROWSER_EXTENSIONS_MEDIA_GALLERIES_HANDLER_H_
