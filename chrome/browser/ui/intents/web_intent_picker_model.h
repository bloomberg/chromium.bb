// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_INTENTS_WEB_INTENT_PICKER_MODEL_H_
#define CHROME_BROWSER_UI_INTENTS_WEB_INTENT_PICKER_MODEL_H_
#pragma once

#include <vector>

#include "base/string16.h"
#include "googleurl/src/gurl.h"
#include "ui/gfx/image/image.h"

class WebIntentPickerModelObserver;

namespace gfx {
class Image;
}

// Model for the WebIntentPicker.
class WebIntentPickerModel {
 public:
  // The intent service disposition.
  // TODO(gbillock): use the webkit_glue::WebIntentServiceData::Disposition
  enum Disposition {
    DISPOSITION_WINDOW,  // Display the intent service in a new window.
    DISPOSITION_INLINE,  // Display the intent service in the picker.
  };

  // An intent service to display in the picker.
  struct InstalledService {
    InstalledService(const string16& title,
                     const GURL& url,
                     Disposition disposition);
    ~InstalledService();

    // The title of this service.
    string16 title;

    // The URL of this service.
    GURL url;

    // A favicon of this service.
    gfx::Image favicon;

    // The disposition to use when displaying this service.
    Disposition disposition;
  };

  // A suggested extension to display in the picker.
  struct SuggestedExtension {
    SuggestedExtension(const string16& title,
                       const string16& id,
                       double average_rating);
    ~SuggestedExtension();

    // The title of the intent service.
    string16 title;

    // The id of the extension that provides the intent service.
    string16 id;

    // The average rating of the extension.
    double average_rating;

    // The extension's icon.
    gfx::Image icon;
  };

  WebIntentPickerModel();
  ~WebIntentPickerModel();

  void set_observer(WebIntentPickerModelObserver* observer) {
    observer_ = observer;
  }

  void set_action(const string16& action) { action_ = action; }

  const string16& action() { return action_; }

  void set_mimetype(const string16& mimetype) { mimetype_ = mimetype; }

  const string16& mimetype() { return mimetype_; }

  void set_default_service_url(const GURL& default_url) {
    default_service_url_ = default_url;
  }

  const GURL& default_service_url() { return default_service_url_; }

  // Add a new installed service with |title|, |url| and |disposition| to the
  // picker.
  void AddInstalledService(const string16& title,
                           const GURL& url,
                           Disposition disposition);

  // Remove an installed service from the picker at |index|.
  void RemoveInstalledServiceAt(size_t index);

  // Remove all installed services from the picker, and resets to not
  // displaying inline disposition.  Note that this does not clear the
  // observer.
  void Clear();

  // Return the intent service installed at |index|.
  const InstalledService& GetInstalledServiceAt(size_t index) const;

  // Return the intent service that uses |url| as its service url, or NULL.
  const InstalledService* GetInstalledServiceWithURL(const GURL& url) const;

  // Return the number of intent services in the picker.
  size_t GetInstalledServiceCount() const;

  // Update the favicon for the intent service at |index| to |image|.
  void UpdateFaviconAt(size_t index, const gfx::Image& image);

  // Add a new suggested extension with |id|, |title| and |average_rating| to
  // the picker.
  void AddSuggestedExtension(const string16& title,
                             const string16& id,
                             double average_rating);

  // Remove a suggested extension from the picker at |index|.
  void RemoveSuggestedExtensionAt(size_t index);

  // Return the suggested extension at |index|.
  const SuggestedExtension& GetSuggestedExtensionAt(size_t index) const;

  // Return the number of suggested extensions.
  size_t GetSuggestedExtensionCount() const;

  // Set the icon image for the suggested extension with |id|.
  void SetSuggestedExtensionIconWithId(const string16& id,
                                       const gfx::Image& image);

  // Set the picker to display the intent service with |url| inline.
  void SetInlineDisposition(const GURL& url);

  // Returns true if the picker is currently displaying an inline service.
  bool IsInlineDisposition() const;

  // Returns the url of the intent service that is being displayed inline, or
  // GURL::EmptyGURL() if none.
  GURL inline_disposition_url() const { return inline_disposition_url_; }

 private:
  // Delete all elements in |installed_services_| and |suggested_extensions_|.
  // Note that this method does not reset the observer.
  void DestroyAll();

  // A vector of all installed services in the picker. Each installed service
  // is owned by this model.
  std::vector<InstalledService*> installed_services_;

  // A vector of all suggested extensions in the picker. Each element is owned
  // by this model.
  std::vector<SuggestedExtension*> suggested_extensions_;

  // The observer to send notifications to, or NULL if none.
  WebIntentPickerModelObserver* observer_;

  // The url of the intent service that is being displayed inline, or
  // GURL::EmptyGURL() if none.
  GURL inline_disposition_url_;

  // A cached copy of the action that instantiated the picker.
  string16 action_;

  // A cached copy of the mimetype that instantiated the picker.
  string16 mimetype_;

  // The non-empty url of the default service if the WebIntentsRegistry
  // finds a default service matching the intent being dispatched.
  GURL default_service_url_;

  DISALLOW_COPY_AND_ASSIGN(WebIntentPickerModel);
};

#endif  // CHROME_BROWSER_UI_INTENTS_WEB_INTENT_PICKER_MODEL_H_
