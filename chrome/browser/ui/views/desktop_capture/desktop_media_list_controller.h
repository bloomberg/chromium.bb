// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_LIST_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_LIST_CONTROLLER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/media/webrtc/desktop_media_list.h"
#include "chrome/browser/media/webrtc/desktop_media_list_observer.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_source_view.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

class DesktopMediaListView;
class DesktopMediaPickerDialogView;

// This class is the controller for a DesktopMediaListView. It is responsible
// for:
//   * Observing a DesktopMediaList
//   * Updating its internal view (currently a DesktopMediaListView) when that
//     DesktopMediaList changes
//   * Providing access to the state of its internal view to the dialog
//   * Proxying between its internal view's callbacks and the dialog's callbacks
class DesktopMediaListController : public DesktopMediaListObserver,
                                   public views::ViewObserver {
 public:
  DesktopMediaListController(DesktopMediaPickerDialogView* dialog,
                             std::unique_ptr<DesktopMediaList> media_list);
  ~DesktopMediaListController() override;

  // Create this controller's corresponding View. There can only be one View per
  // controller; attempting to call this method twice on the same
  // DesktopMediaListController is not allowed.
  std::unique_ptr<views::View> CreateView(
      DesktopMediaSourceViewStyle generic_style,
      DesktopMediaSourceViewStyle single_style,
      const base::string16& accessible_name);

  // Starts observing the DesktopMediaList given earlier, ignoring any entries
  // whose id matches dialog_window_id.
  void StartUpdating(content::DesktopMediaID dialog_window_id);

  // Focuses this controller's view.
  void FocusView();

  // Returns the DesktopMediaID corresponding to the current selection in this
  // controller's view, if there is one.
  base::Optional<content::DesktopMediaID> GetSelection() const;

  // These three methods are called by the view to inform the controller of
  // events. The first two indicate changes in the visual state of the view; the
  // last indicates that the user performed an action on the source view that
  // should serve to accept the entire dialog.
  void OnSourceListLayoutChanged();
  void OnSourceSelectionChanged();
  void AcceptSource();

  // Returns the view that should receive initial focus in the dialog if this
  // controller's source list is the one being shown.
  views::View* GetViewForInitialFocus();

  // These two methods are used by the view (or its subviews) to query and
  // update the underlying DesktopMediaList.
  const DesktopMediaList::Source& GetSource(int index) const;
  void SetThumbnailSize(const gfx::Size& size);

 private:
  friend class DesktopMediaPickerViewsTestApi;

  // This method is used as a callback to support source auto-selection, which
  // is used in some tests; it acts as though the user had selected (in the
  // view) the element corresponding to the given source and accepted the
  // dialog.
  void AcceptSpecificSource(content::DesktopMediaID source);

  // DesktopMediaListObserver:
  void OnSourceAdded(DesktopMediaList* list, int index) override;
  void OnSourceRemoved(DesktopMediaList* list, int index) override;
  void OnSourceMoved(DesktopMediaList* list,
                     int old_index,
                     int new_index) override;
  void OnSourceNameChanged(DesktopMediaList* list, int index) override;
  void OnSourceThumbnailChanged(DesktopMediaList* list, int index) override;

  // ViewObserver:
  void OnViewIsDeleting(views::View* view) override;

  DesktopMediaPickerDialogView* dialog_;
  std::unique_ptr<DesktopMediaList> media_list_;
  DesktopMediaListView* view_{nullptr};
  ScopedObserver<views::View, views::ViewObserver> view_observer_{this};

  base::WeakPtrFactory<DesktopMediaListController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_LIST_CONTROLLER_H_
