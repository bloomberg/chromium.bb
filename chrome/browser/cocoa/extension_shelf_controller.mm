// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "extension_shelf_controller.h"

#include "base/mac_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/extensions/extension_shelf_model.h"
#include "skia/ext/skia_utils_mac.h"

namespace {

const int kExtensionShelfPaddingTop = 1;
const int kToolstripPadding = 2;

}

// This class manages the extensions ("toolstrips") on the shelf. It listens to
// events sent by the extension system, and acts as a bridge between that and
// the cocoa world.
class ExtensionShelfMac : public ExtensionShelfModelObserver {
 public:
  ExtensionShelfMac(Browser* browser, ExtensionShelfController* controller);
  virtual ~ExtensionShelfMac();

  // ExtensionShelfModelObserver
  virtual void ToolstripInsertedAt(ExtensionHost* toolstrip, int index);
  virtual void ToolstripRemovingAt(ExtensionHost* toolstrip, int index);
  virtual void ToolstripMoved(ExtensionHost* toolstrip,
                              int from_index,
                              int to_index);
  virtual void ToolstripChangedAt(ExtensionHost* toolstrip, int index);
  virtual void ExtensionShelfEmpty();
  virtual void ShelfModelReloaded();
  virtual void ShelfModelDeleting();

  // Determines what is our target height and sets it.
  void AdjustHeight();

 private:
  class Toolstrip;

  void Show();
  void Hide();

  // Create the contents of the extension shelf.
  void Init(Profile* profile);

  // Loads the background image into memory, or does nothing if already loaded.
  void InitBackground();

  // Re-inserts all toolstrips from the model. Must be called when the shelf
  // contains no toolstrips.
  void LoadFromModel();

  void DeleteToolstrips();

  Toolstrip* ToolstripAtIndex(int index);

  ExtensionShelfController* controller_;  // weak, owns us

  Browser* browser_;  // weak

  // Lazily-initialized background for toolstrips.
  scoped_ptr<SkBitmap> background_;

  // The model representing the toolstrips on the shelf.
  ExtensionShelfModel* model_;  // weak

  // Set of toolstrip views which are really on the shelf.
  std::set<Toolstrip*> toolstrips_;

  // Stores if we are currently layouting items.
  bool is_adjusting_height_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionShelfMac);
};

// This class represents a single extension ("toolstrip") on the extension
// shelf.
class ExtensionShelfMac::Toolstrip {
 public:
  explicit Toolstrip(ExtensionHost* host)
      : host_(host) {
    DCHECK(host_->view());
    Init();
  }

  // Inserts the native NSView belonging to this extension into the view that
  // belongs to |controller|. Makes sure the controller is notified when the
  // extension's |frame| changes.
  void AddToolstripToController(ExtensionShelfController* controller);

  // Removes the native NSView belonging to this extension from the view that
  // belongs to |controller|. Removes |controller| as a frame size observer.
  void RemoveToolstripFromController(ExtensionShelfController* controller);

  // Sets the image that is used by the extension.
  void SetBackground(const SkBitmap& background) {
    host_->view()->SetBackground(background);
  }

  // Returns the native NSView belonging to this extension.
  gfx::NativeView native_view() {
    return host_->view()->native_view();
  }

 private:
  void Init();

  ExtensionHost* host_;  // weak

  const std::string extension_name_;

 private:
  DISALLOW_COPY_AND_ASSIGN(Toolstrip);
};

void ExtensionShelfMac::Toolstrip::AddToolstripToController(
    ExtensionShelfController* controller) {
  NSView* toolstrip_view = host_->view()->native_view();
  [[controller view] addSubview:toolstrip_view];

  [[NSNotificationCenter defaultCenter]
      addObserver:controller
         selector:@selector(updateVisibility:)
             name:NSViewFrameDidChangeNotification
           object:toolstrip_view];
}

void ExtensionShelfMac::Toolstrip::RemoveToolstripFromController(
    ExtensionShelfController* controller) {
  [host_->view()->native_view() removeFromSuperview];

  [[NSNotificationCenter defaultCenter]
      removeObserver:controller
                name:NSViewFrameDidChangeNotification
              object:host_->view()->native_view()];
}

void ExtensionShelfMac::Toolstrip::Init() {
  host_->view()->set_is_toolstrip(true);
}

ExtensionShelfMac::ExtensionShelfMac(Browser* browser,
                                     ExtensionShelfController* controller)
    : controller_(controller),
      browser_(browser),
      model_(browser->extension_shelf_model()),
      is_adjusting_height_(false) {
  if (model_)  // Can be NULL in tests.
    Init(browser_->profile());
}

ExtensionShelfMac::~ExtensionShelfMac() {
  DeleteToolstrips();
  if (model_)
    model_->RemoveObserver(this);
}

void ExtensionShelfMac::Show() {
  [controller_ show:nil];
}

void ExtensionShelfMac::Hide() {
  [controller_ hide:nil];
}

void ExtensionShelfMac::ToolstripInsertedAt(ExtensionHost* host,
                                            int index) {
  InitBackground();
  Toolstrip* toolstrip = new Toolstrip(host);
  toolstrip->SetBackground(*background_.get());
  toolstrip->AddToolstripToController(controller_);
  toolstrips_.insert(toolstrip);
  model_->SetToolstripDataAt(index, toolstrip);

  AdjustHeight();
}

void ExtensionShelfMac::ToolstripRemovingAt(ExtensionHost* host,
                                            int index) {
  Toolstrip* toolstrip = ToolstripAtIndex(index);
  toolstrip->RemoveToolstripFromController(controller_);
  toolstrips_.erase(toolstrip);
  model_->SetToolstripDataAt(index, NULL);
  delete toolstrip;

  AdjustHeight();
}

void ExtensionShelfMac::ToolstripMoved(ExtensionHost* host,
                                       int from_index,
                                       int to_index) {
  // TODO(thakis): Implement reordering toolstrips.
  AdjustHeight();
}

void ExtensionShelfMac::ToolstripChangedAt(
    ExtensionHost* toolstrip, int index) {
  // TODO(thakis): Implement changing toolstrips.
  AdjustHeight();
}

void ExtensionShelfMac::ExtensionShelfEmpty() {
  AdjustHeight();
}

void ExtensionShelfMac::ShelfModelReloaded() {
  DeleteToolstrips();
  LoadFromModel();
}

void ExtensionShelfMac::ShelfModelDeleting() {
  DeleteToolstrips();
  model_->RemoveObserver(this);
  model_ = NULL;
}

void ExtensionShelfMac::Init(Profile* profile) {
  LoadFromModel();
  model_->AddObserver(this);
}

void ExtensionShelfMac::InitBackground() {
  if (background_.get())
    return;

  // If this is called while the shelf is invisible, shortly resize the shelf so
  // that it can paint itself.
  NSRect current_frame = [[controller_ view] frame];
  if (current_frame.size.height < [controller_ height]) {
    NSRect new_frame = current_frame;
    new_frame.size.height = [controller_ height];
    [[controller_ view] setFrame:new_frame];
  }

  // The background is tiled horizontally in the toolstrip. Hence, its width
  // should not be too small so that tiling is fast, and not too large, so that
  // not too much memory is needed -- but the exact width doesn't really matter.
  const CGFloat kBackgroundTileWidth = 100;

  // Paint shelf background into an SkBitmap. If we decide to keep the shelf, we
  // need to do this for both the "main window" and "not main window" shadings.
  NSRect background_rect = NSMakeRect(
      0, 0,
      kBackgroundTileWidth, [controller_ height] - kExtensionShelfPaddingTop);
  NSBitmapImageRep* bitmap_rep = [[controller_ view]
      bitmapImageRepForCachingDisplayInRect:background_rect];

  [[controller_ view] cacheDisplayInRect:background_rect
                        toBitmapImageRep:bitmap_rep];
  background_.reset(new SkBitmap(gfx::CGImageToSkBitmap([bitmap_rep CGImage])));

  // Restore old frame.
  [[controller_ view] setFrame:current_frame];
}

void ExtensionShelfMac::AdjustHeight() {
  if (model_->empty() || toolstrips_.empty()) {
    // It's possible that |model_| is not empty, but |toolstrips_| are empty
    // when removing the last toolstrip.
    DCHECK(toolstrips_.empty());
    Hide();
    return;
  }

  if (is_adjusting_height_)
    return;
  is_adjusting_height_ = true;

  Show();

  // Lay out items horizontally from left to right. This method's name is
  // misleading, but matches linux and windows for now.
  CGFloat x = 0;
  for (std::set<Toolstrip*>::iterator iter = toolstrips_.begin();
       iter != toolstrips_.end(); ++iter) {
    NSView* view = (*iter)->native_view();
    NSRect frame = [view frame];
    frame.origin.x = x;
    frame.origin.y = 0;
    frame.size.height = [controller_ height] - kExtensionShelfPaddingTop;
    [view setFrame:frame];
    x += frame.size.width + kToolstripPadding;
  }

  is_adjusting_height_ = false;
}

void ExtensionShelfMac::LoadFromModel() {
  DCHECK(toolstrips_.empty());
  int count = model_->count();
  for (int i = 0; i < count; ++i)
    ToolstripInsertedAt(model_->ToolstripAt(i).host, i);
  AdjustHeight();
}

void ExtensionShelfMac::DeleteToolstrips() {
  for (std::set<Toolstrip*>::iterator iter = toolstrips_.begin();
       iter != toolstrips_.end(); ++iter) {
    (*iter)->RemoveToolstripFromController(controller_);
    delete *iter;
  }
  toolstrips_.clear();
}

ExtensionShelfMac::Toolstrip* ExtensionShelfMac::ToolstripAtIndex(int index) {
  return static_cast<Toolstrip*>(model_->ToolstripAt(index).data);
}


@implementation ExtensionShelfController

- (id)initWithBrowser:(Browser*)browser
       resizeDelegate:(id<ViewResizer>)resizeDelegate {
  if ((self = [super initWithNibName:@"ExtensionShelf"
                              bundle:mac_util::MainAppBundle()])) {
    resizeDelegate_ = resizeDelegate;
    browser_ = browser;
    shelfHeight_ = [[self view] bounds].size.height;

    NSRect frame = [[self view] frame];
    frame.size.height = 0;
    [[self view] setFrame:frame];
  }
  return self;
}

- (void)wasInsertedIntoWindow {
  // The bridge_ calls cacheDisplayInRect:toBitmapImageRep:, which requires that
  // the view is in a superview to work. Hence, create the bridge object no
  // sooner.
  DCHECK(bridge_.get() == NULL);
  bridge_.reset(new ExtensionShelfMac(browser_, self));
}

- (IBAction)show:(id)sender {
  [resizeDelegate_ resizeView:[self view] newHeight:shelfHeight_];
}

- (IBAction)hide:(id)sender {
  [resizeDelegate_ resizeView:[self view] newHeight:0];
}

- (CGFloat)height {
  return shelfHeight_;
}

- (void)updateVisibility:(id)sender {
  if (bridge_.get())
    bridge_->AdjustHeight();
}

@end
