// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SHELF_MODEL_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SHELF_MODEL_H_

#include <vector>

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extensions_service.h"

class Browser;
class ExtensionPrefs;
class ExtensionShelfModelObserver;

// The model representing the toolstrips on an ExtensionShelf.  The order of
// the toolstrips is common across all of the models for a given Profile,
// but there are multiple models.  Each model contains the hosts/views which
// are specific to a Browser.
class ExtensionShelfModel : public NotificationObserver {
 public:
  explicit ExtensionShelfModel(Browser* browser);
  virtual ~ExtensionShelfModel();

  struct ToolstripItem {
    ExtensionHost* host;
    Extension::ToolstripInfo info;
    void* data;
    int height;
    GURL url;
  };

  typedef std::vector<ToolstripItem> ToolstripList;
  typedef ToolstripList::iterator iterator;

  // Add and remove observers to changes within this ExtensionShelfModel.
  void AddObserver(ExtensionShelfModelObserver* observer);
  void RemoveObserver(ExtensionShelfModelObserver* observer);

  // The number of toolstrips in the model.
  int count() const { return static_cast<int>(toolstrips_.size()); }
  bool empty() const { return toolstrips_.empty(); }

  // Iterators for the toolstrips in the model.
  iterator begin() { return toolstrips_.begin(); }
  ExtensionShelfModel::iterator end() { return toolstrips_.end(); }

  // Add |toolstrip| to the end of the shelf.
  void AppendToolstrip(const ToolstripItem& toolstrip);

  // Insert |toolstrip| and |data| at |index|.
  void InsertToolstripAt(int index, const ToolstripItem& toolstrip);

  // Remove the toolstrip at |index|.
  void RemoveToolstripAt(int index);

  // Move the toolstrip at |index| to |to_index|.
  void MoveToolstripAt(int index, int to_index);

  // Lookup the index of |host|.  Returns -1 if not present.
  int IndexOfHost(ExtensionHost* host);

  // Return the toolstrip at |index|.
  const ToolstripItem& ToolstripAt(int index);

  // Return the ToolstripItem associated with |host| or NULL if it's not
  // present.
  ToolstripList::iterator ToolstripForHost(ExtensionHost* host);

  // Set some arbitrary data associated with a particular toolstrip.
  void SetToolstripDataAt(int index, void* data);

  // Update the ToolstripItem for |toolstrip| to set its |url| and |height|
  // and then call ToolstripChanged for all observers.
  // If |url| is empty, no navigation is requested.
  void ExpandToolstrip(iterator toolstrip, const GURL& url, int height);

  // Update the ToolstripItem for |toolstrip| to set its |url| and its height
  // to 0, and then call ToolstripChanged for all observers.
  // If |url| is empty, no navigation is requested.
  void CollapseToolstrip(iterator toolstrip, const GURL& url);

  // NotificationObserver
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  // Add all of the toolstrips from |extension|.
  void AddExtension(Extension* extension);

  // Add all of the toolstrips from each extension in |extensions|.
  void AddExtensions(const ExtensionList* extensions);

  // Remove all of the toolstrips in |extension| from the shelf.
  void RemoveExtension(Extension* extension);

  // Update prefs with the most recent changes.
  void UpdatePrefs();

  // Reloads order from prefs.
  void SortToolstrips();

  // The browser that this model is attached to.
  Browser* browser_;

  // The preferences that this model uses.
  ExtensionPrefs* prefs_;

  // Manages our notification registrations.
  NotificationRegistrar registrar_;

  // The Toolstrips loaded in this model. The model owns these objects.
  ToolstripList toolstrips_;

  // Our observers.
  typedef ObserverList<ExtensionShelfModelObserver>
      ExtensionShelfModelObservers;
  ExtensionShelfModelObservers observers_;

  // Whether the model has received an EXTENSIONS_READY notification.
  bool ready_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionShelfModel);
};

// Objects implement this interface when they wish to be notified of changes to
// the ExtensionShelfModel.
//
// Register your ExtensionShelfModelObserver with the ExtensionShelfModel using
// Add/RemoveObserver methods.
class ExtensionShelfModelObserver {
 public:
  // A new toolstrip was inserted into ExtensionShelfModel at |index|.
  virtual void ToolstripInsertedAt(ExtensionHost* toolstrip, int index) {}

  // The specified toolstrip is being removed and destroyed.
  virtual void ToolstripRemovingAt(ExtensionHost* toolstrip, int index) {}

  // |toolstrip| moved from |from_index| to |to_index|.
  virtual void ToolstripMoved(ExtensionHost* toolstrip,
                              int from_index,
                              int to_index) {}

  // The specified toolstrip changed in some way (currently only size changes)
  virtual void ToolstripChanged(ExtensionShelfModel::iterator toolstrip) {}

  // There are no more toolstrips in the model.
  virtual void ExtensionShelfEmpty() {}

  // The entire model may have changed.
  virtual void ShelfModelReloaded() {}

  // The model is being destroyed.
  virtual void ShelfModelDeleting() {}
};


#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SHELF_MODEL_H_
