// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reader_mode/reader_mode_controller.h"

#include <memory>
#include <utility>

#include "base/mac/bind_objc_block.h"
#include "base/memory/ptr_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/dom_distiller/distiller_viewer.h"
#include "ios/chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/ui/reader_mode/reader_mode_checker.h"
#import "ios/chrome/browser/ui/reader_mode/reader_mode_infobar_delegate.h"
#import "ios/chrome/browser/ui/reader_mode/reader_mode_view.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/web_state/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@protocol ReaderModeCheckerObserverBridgeProtocol
- (void)pageIsDistillable;
@end

@protocol InfoBarManagerObserverBridgeProtocol
- (void)infoBarRemoved:(infobars::InfoBar*)infobar;
@end

namespace {
// Used to find the ReaderModeView in the view hierarchy.
const NSInteger kReaderModeViewTag = 42;
const CGFloat kReaderModeAnimationDuration = .5;

class ReaderModeCheckerObserverBridge : ReaderModeCheckerObserver {
 public:
  ReaderModeCheckerObserverBridge(
      ReaderModeChecker* readerModeChecker,
      id<ReaderModeCheckerObserverBridgeProtocol> observer)
      : ReaderModeCheckerObserver(readerModeChecker), observer_(observer) {
    DCHECK(observer);
  };

  void PageIsDistillable() override { [observer_ pageIsDistillable]; };

 private:
  id<ReaderModeCheckerObserverBridgeProtocol> observer_;
};

class InfoBarManagerObserverBridge : infobars::InfoBarManager::Observer {
 public:
  InfoBarManagerObserverBridge(
      infobars::InfoBarManager* infoBarManager,
      id<InfoBarManagerObserverBridgeProtocol> observer)
      : infobars::InfoBarManager::Observer(),
        manager_(infoBarManager),
        observer_(observer) {
    DCHECK(infoBarManager);
    DCHECK(observer);
    manager_->AddObserver(this);
  };

  ~InfoBarManagerObserverBridge() override {
    if (manager_)
      manager_->RemoveObserver(this);
  }

  void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override {
    [observer_ infoBarRemoved:infobar];
  }

  void OnManagerShuttingDown(infobars::InfoBarManager* manager) override {
    manager_->RemoveObserver(this);
    manager_ = nullptr;
  }

 private:
  infobars::InfoBarManager* manager_;
  id<InfoBarManagerObserverBridgeProtocol> observer_;
};
}  // namespace

@interface ReaderModeController ()<ReaderModeViewDelegate,
                                   ReaderModeCheckerObserverBridgeProtocol,
                                   InfoBarManagerObserverBridgeProtocol> {
  std::unique_ptr<ReaderModeChecker> _checker;
  std::unique_ptr<ReaderModeCheckerObserverBridge> _checkerBridge;
  std::unique_ptr<InfoBarManagerObserverBridge> _infoBarBridge;
  std::unique_ptr<dom_distiller::DistillerViewer> _viewer;
  // The currently displayed infobar.
  infobars::InfoBar* infobar_;
  web::WebState* _webState;
}
@property(weak, readonly, nonatomic) id<ReaderModeControllerDelegate> delegate;

// Triggers a distillation and returns a DistillerViewer to keep as a handle to
// the running distillation.
- (std::unique_ptr<dom_distiller::DistillerViewer>)startDistillation
    WARN_UNUSED_RESULT;
- (void)distillationFinished:(const std::string&)html forURL:(const GURL&)url;

// Returns a ReaderModeView that presents a waiting UI while the distillation
// is taking place. Releasing this view will stop the distillation in progress.
- (ReaderModeView*)readerModeViewWithFrame:(CGRect)frame;

- (void)showInfoBar:(const std::string&)html forURL:(const GURL&)url;
- (void)removeInfoBar;

@end

@implementation ReaderModeController
@synthesize delegate = _delegate;

- (instancetype)initWithWebState:(web::WebState*)webState
                        delegate:(id<ReaderModeControllerDelegate>)delegate {
  DCHECK(webState);
  self = [super init];
  if (self) {
    _webState = webState;
    _checker = base::MakeUnique<ReaderModeChecker>(_webState);
    _delegate = delegate;
    _checkerBridge =
        base::MakeUnique<ReaderModeCheckerObserverBridge>(_checker.get(), self);
    infobars::InfoBarManager* infobar_manager =
        InfoBarManagerImpl::FromWebState(_webState);
    _infoBarBridge =
        base::MakeUnique<InfoBarManagerObserverBridge>(infobar_manager, self);
  }
  return self;
}

- (instancetype)init {
  NOTREACHED();
  return nil;
}

- (void)dealloc {
  if (_webState)
    [self detachFromWebState];
}

- (void)detachFromWebState {
  [self removeInfoBar];
  _webState = nullptr;
}

// Property accessor.
- (ReaderModeChecker*)checker {
  return _checker.get();
}

#pragma mark - Private methods.
#pragma mark distillation

- (std::unique_ptr<dom_distiller::DistillerViewer>)startDistillation {
  DCHECK(_webState);
  __weak ReaderModeController* weakSelf = self;
  GURL pageURL = _webState->GetLastCommittedURL();
  ios::ChromeBrowserState* browserState =
      ios::ChromeBrowserState::FromBrowserState(_webState->GetBrowserState());
  return base::MakeUnique<dom_distiller::DistillerViewer>(
      dom_distiller::DomDistillerServiceFactory::GetForBrowserState(
          browserState),
      browserState->GetPrefs(), pageURL,
      base::BindBlockArc(^(
          const GURL& pageURL, const std::string& html,
          const std::vector<dom_distiller::DistillerViewer::ImageInfo>& images,
          const std::string& title) {
        [weakSelf distillationFinished:html forURL:pageURL];
      }));
}

- (void)distillationFinished:(const std::string&)html forURL:(const GURL&)url {
  UIView* superview = [self.delegate superviewForReaderModePanel];
  DCHECK(_viewer || [superview viewWithTag:kReaderModeViewTag]);
  if ([superview viewWithTag:kReaderModeViewTag]) {
    [self.delegate loadReaderModeHTML:base::SysUTF8ToNSString(html) forURL:url];
  } else if (_viewer) {
    [self showInfoBar:html forURL:url];
  }
}

#pragma mark view creation

- (ReaderModeView*)readerModeViewWithFrame:(CGRect)frame {
  DCHECK(_checker->CanSwitchToReaderMode());
  ReaderModeView* view =
      [[ReaderModeView alloc] initWithFrame:frame delegate:self];
  [view assignDistillerViewer:[self startDistillation]];
  return view;
}

#pragma mark infobar.

- (void)showInfoBar:(const std::string&)html forURL:(const GURL&)url {
  DCHECK(_webState);
  __weak id<ReaderModeControllerDelegate> weakDelegate = self.delegate;

  // Non reference version of the variables needed.
  const std::string html_non_ref(html);
  const GURL url_non_ref(url);
  auto infoBarDelegate =
      base::MakeUnique<ReaderModeInfoBarDelegate>(base::BindBlockArc(^{
        [weakDelegate loadReaderModeHTML:base::SysUTF8ToNSString(html_non_ref)
                                  forURL:url_non_ref];
      }));

  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(_webState);

  std::unique_ptr<infobars::InfoBar> infobar =
      infobar_manager->CreateConfirmInfoBar(std::move(infoBarDelegate));
  if (infobar_)
    infobar_ = infobar_manager->ReplaceInfoBar(infobar_, std::move(infobar));
  else
    infobar_ = infobar_manager->AddInfoBar(std::move(infobar));
}

- (void)removeInfoBar {
  DCHECK(_webState);
  if (infobar_) {
    infobars::InfoBarManager* infobar_manager =
        InfoBarManagerImpl::FromWebState(_webState);
    infobar_manager->RemoveInfoBar(infobar_);
    infobar_ = nullptr;
  }
}

#pragma mark - public methods.

- (void)switchToReaderMode {
  UIView* superview = [self.delegate superviewForReaderModePanel];
  if ([superview viewWithTag:kReaderModeViewTag])
    return;  // There is already a reader mode waiting view visible.

  [self removeInfoBar];

  // Get the view.
  ReaderModeView* readerView = [self readerModeViewWithFrame:superview.bounds];
  readerView.tag = kReaderModeViewTag;
  [superview addSubview:readerView];

  // Animate the view in. First the view is animated in (via transparency) and
  // the the animation on the view itself is started.
  readerView.alpha = 0.0;

  [UIView animateWithDuration:kReaderModeAnimationDuration
      delay:0.0
      options:UIViewAnimationOptionCurveEaseIn
      animations:^{
        readerView.alpha = 1.0;
      }
      completion:^(BOOL finished) {
        if (finished)
          [readerView start];  // Starts the waiting animation on the view.
      }];
}

#pragma mark - ReaderModeViewDelegate.

- (void)exitReaderMode {
  UIView* superview = [self.delegate superviewForReaderModePanel];
  if (![superview viewWithTag:kReaderModeViewTag]) {
    DCHECK(_viewer);
    return;
  }

  ReaderModeView* readerView =
      static_cast<ReaderModeView*>([superview viewWithTag:kReaderModeViewTag]);
  if (!readerView)
    return;

  // First stop the view waiting animation (if any) and then remove the view.
  [readerView stopWaitingWithCompletion:^{
    [UIView animateWithDuration:kReaderModeAnimationDuration
        delay:0.0
        options:UIViewAnimationOptionCurveEaseIn
        animations:^{
          readerView.alpha = 0.0;
        }
        completion:^(BOOL finished) {
          [readerView removeFromSuperview];
        }];
  }];
}

#pragma mark - ReaderModeCheckerObserverBridgeProtocol

- (void)pageIsDistillable {
  _viewer = [self startDistillation];
}

#pragma mark - InfoBarManagerObserverBridgeProtocol.

- (void)infoBarRemoved:(infobars::InfoBar*)infobar {
  if (infobar == infobar_) {
    _viewer.reset();
    infobar_ = nullptr;
  }
}

@end
