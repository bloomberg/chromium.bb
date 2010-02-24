// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_shelf_model.h"

#include "base/stl_util-inl.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_toolstrip_api.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_service.h"

ExtensionShelfModel::ExtensionShelfModel(Browser* browser)
    : browser_(browser), ready_(false) {
  // Watch extensions loaded and unloaded notifications.
  registrar_.Add(this, NotificationType::EXTENSION_UNLOADED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::EXTENSION_LOADED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::EXTENSIONS_READY,
                 NotificationService::AllSources());

  // Add any already-loaded extensions now, since we missed the notification for
  // those.
  ExtensionsService* service = browser_->profile()->GetExtensionsService();
  if (service) {  // This can be null in unit tests.
    prefs_ = browser_->profile()->GetExtensionsService()->extension_prefs();
    registrar_.Add(this, NotificationType::EXTENSION_SHELF_MODEL_CHANGED,
                   Source<ExtensionPrefs>(prefs_));
    ready_ = service->is_ready();
    if (ready_) {
      AddExtensions(service->extensions());
      SortToolstrips();
    }
  }
}

ExtensionShelfModel::~ExtensionShelfModel() {
  FOR_EACH_OBSERVER(ExtensionShelfModelObserver, observers_,
                    ShelfModelDeleting());

  observers_.Clear();

  for (iterator t = toolstrips_.begin(); t != toolstrips_.end(); ++t)
    delete t->host;
  toolstrips_.clear();
}

void ExtensionShelfModel::AddObserver(ExtensionShelfModelObserver* observer) {
  observers_.AddObserver(observer);
}

void ExtensionShelfModel::RemoveObserver(
    ExtensionShelfModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

void ExtensionShelfModel::AppendToolstrip(const ToolstripItem& toolstrip) {
  InsertToolstripAt(count(), toolstrip);
}

void ExtensionShelfModel::InsertToolstripAt(int index,
                                            const ToolstripItem& toolstrip) {
  toolstrips_.insert(toolstrips_.begin() + index, toolstrip);
  if (ready_) {
    FOR_EACH_OBSERVER(ExtensionShelfModelObserver, observers_,
                      ToolstripInsertedAt(toolstrip.host, index));
  }
}

void ExtensionShelfModel::RemoveToolstripAt(int index) {
  ExtensionHost* host = ToolstripAt(index).host;
  FOR_EACH_OBSERVER(ExtensionShelfModelObserver, observers_,
                    ToolstripRemovingAt(host, index));
  toolstrips_.erase(toolstrips_.begin() + index);
  delete host;
}

void ExtensionShelfModel::MoveToolstripAt(int index, int to_index) {
  DCHECK(index >= 0);
  DCHECK(to_index >= 0);
  if (index == to_index)
    return;

  ToolstripItem toolstrip = toolstrips_[index];
  toolstrips_.erase(toolstrips_.begin() + index);
  toolstrips_.insert(toolstrips_.begin() + to_index, toolstrip);

  FOR_EACH_OBSERVER(ExtensionShelfModelObserver, observers_,
                    ToolstripMoved(toolstrip.host, index, to_index));

  UpdatePrefs();
}

int ExtensionShelfModel::IndexOfHost(ExtensionHost* host) {
  for (iterator i = toolstrips_.begin(); i != toolstrips_.end(); ++i) {
    if (i->host == host)
      return i - toolstrips_.begin();
  }
  return -1;
}

ExtensionShelfModel::iterator ExtensionShelfModel::ToolstripForHost(
    ExtensionHost* host) {
  for (iterator i = toolstrips_.begin(); i != toolstrips_.end(); ++i) {
    if (i->host == host)
      return i;
  }
  return toolstrips_.end();
}

const ExtensionShelfModel::ToolstripItem& ExtensionShelfModel::ToolstripAt(
    int index) {
  DCHECK(index >= 0);
  return toolstrips_[index];
}

void ExtensionShelfModel::SetToolstripDataAt(int index, void* data) {
  DCHECK(index >= 0);
  toolstrips_[index].data = data;
}

void ExtensionShelfModel::ExpandToolstrip(iterator toolstrip,
                                          const GURL& url, int height) {
  if (toolstrip == end())
    return;
  toolstrip->height = height;
  toolstrip->url = url;
  FOR_EACH_OBSERVER(ExtensionShelfModelObserver, observers_,
                    ToolstripChanged(toolstrip));
  int routing_id = toolstrip->host->render_view_host()->routing_id();
  ToolstripEventRouter::OnToolstripExpanded(browser_->profile(),
                                            routing_id,
                                            url, height);
  toolstrip->host->SetRenderViewType(ViewType::EXTENSION_MOLE);
}

void ExtensionShelfModel::CollapseToolstrip(iterator toolstrip,
                                            const GURL& url) {
  if (toolstrip == end())
    return;
  toolstrip->height = 0;
  toolstrip->url = url;
  FOR_EACH_OBSERVER(ExtensionShelfModelObserver, observers_,
                    ToolstripChanged(toolstrip));
  int routing_id = toolstrip->host->render_view_host()->routing_id();
  ToolstripEventRouter::OnToolstripCollapsed(browser_->profile(),
                                             routing_id,
                                             url);
  toolstrip->host->SetRenderViewType(ViewType::EXTENSION_TOOLSTRIP);
}

void ExtensionShelfModel::Observe(NotificationType type,
                                  const NotificationSource& source,
                                  const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::EXTENSION_LOADED:
      if (ready_)
        AddExtension(Details<Extension>(details).ptr());
      break;
    case NotificationType::EXTENSION_UNLOADED:
      RemoveExtension(Details<Extension>(details).ptr());
      break;
    case NotificationType::EXTENSIONS_READY:
      if (browser_->profile()->GetExtensionsService()) {
        AddExtensions(
            browser_->profile()->GetExtensionsService()->extensions());
        SortToolstrips();
      }
      ready_ = true;
      break;
    case NotificationType::EXTENSION_SHELF_MODEL_CHANGED:
      // Ignore changes that this model originated.
      if (Details<ExtensionShelfModel>(details).ptr() != this)
        SortToolstrips();
      break;
    default:
      DCHECK(false) << "Unhandled notification of type: " << type.value;
      break;
  }
}

void ExtensionShelfModel::AddExtension(Extension* extension) {
  ExtensionProcessManager* manager =
      browser_->profile()->GetExtensionProcessManager();
  DCHECK(manager);
  if (!manager)
    return;

  for (std::vector<Extension::ToolstripInfo>::const_iterator toolstrip =
       extension->toolstrips().begin();
       toolstrip != extension->toolstrips().end(); ++toolstrip) {
    GURL url = toolstrip->toolstrip;
    ToolstripItem item;
    item.host = manager->CreateToolstrip(extension, url, browser_);
    item.info = *toolstrip;
    item.data = NULL;
    item.height = 0;
    AppendToolstrip(item);
  }
}

void ExtensionShelfModel::AddExtensions(const ExtensionList* extensions) {
  if (extensions->size()) {
    ExtensionList::const_iterator extension = extensions->begin();
    for (; extension != extensions->end(); ++extension)
      AddExtension(*extension);
  }
}

void ExtensionShelfModel::RemoveExtension(Extension* extension) {
  bool changed = false;
  for (int i = count() - 1; i >= 0; --i) {
    ExtensionHost* t = ToolstripAt(i).host;
    if (t->extension()->id() == extension->id()) {
      changed = true;
      RemoveToolstripAt(i);

      // There can be more than one toolstrip per extension, so we have to keep
      // looping even after finding a match.
    }
  }
  if (changed)
    UpdatePrefs();
}

void ExtensionShelfModel::UpdatePrefs() {
  if (!prefs_)
    return;

  // It's easiest to just rebuild the list each time.
  ExtensionPrefs::URLList urls;
  for (int i = 0; i < count(); ++i)
    urls.push_back(ToolstripAt(i).host->GetURL());
  prefs_->SetShelfToolstripOrder(urls);

  NotificationService::current()->Notify(
      NotificationType::EXTENSION_SHELF_MODEL_CHANGED,
      Source<ExtensionPrefs>(prefs_),
      Details<ExtensionShelfModel>(this));
}

void ExtensionShelfModel::SortToolstrips() {
  ExtensionPrefs::URLList urls = prefs_->GetShelfToolstripOrder();
  ToolstripList copy =
      ToolstripList(toolstrips_.begin(), toolstrips_.end());
  toolstrips_.clear();

  // Go through the urls and find the matching toolstrip, re-adding it to the
  // new list in the proper order.
  for (size_t i = 0; i < urls.size(); ++i) {
    GURL& url = urls[i];
    for (iterator toolstrip = copy.begin();
        toolstrip != copy.end(); ++toolstrip) {
      if (url == toolstrip->host->GetURL()) {
        // Note that it's technically possible for the same URL to appear in
        // multiple toolstrips, so we don't do any testing for uniqueness.
        toolstrips_.push_back(*toolstrip);

        // Remove the toolstrip from the list so we don't have to iterate over
        // it next time.
        copy.erase(toolstrip);
        break;
      }
    }
  }

  // Any toolstrips remaining in |copy| were somehow missing from the prefs,
  // so just append them to the end.
  for (iterator toolstrip = copy.begin();
      toolstrip != copy.end(); ++toolstrip) {
    toolstrips_.push_back(*toolstrip);
  }

  FOR_EACH_OBSERVER(ExtensionShelfModelObserver, observers_,
                    ShelfModelReloaded());
}
