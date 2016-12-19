// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_PANEL_PROTOCOL_H_
#define IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_PANEL_PROTOCOL_H_

#import <UIKit/UIKit.h>

@protocol NewTabPagePanelProtocol;

extern const int kNewTabPageShadowHeight;
extern const int kNewTabPageDistanceToFadeShadow;

@protocol NewTabPagePanelControllerDelegate<NSObject>

// Updates the NTP bar shadow alpha for the given NewTabPagePanelProtocol.
- (void)updateNtpBarShadowForPanelController:
    (id<NewTabPagePanelProtocol>)ntpPanelController;

@end

// TODO(jbbegue): rename, extract and upstream so that CRWNativeContent can
// implement it ( https://crbug.com/492156 ).
@protocol NewTabPagePanelControllerSnapshotting<NSObject>

@optional
// Called when a snapshot of the content will be taken.
- (void)willUpdateSnapshot;

@end

// Base class of a controller for the panels in the New Tab Page. This should
// not be instantiated, but instead one of its sub-classes.
@protocol NewTabPagePanelProtocol<NewTabPagePanelControllerSnapshotting>

// NewTabPagePanelController delegate, may be nil.
@property(nonatomic, assign) id<NewTabPagePanelControllerDelegate> delegate;

// Alpha value to use for the NewTabPageBar shadow.
@property(nonatomic, readonly) CGFloat alphaForBottomShadow;

// Main view.
@property(nonatomic, readonly) UIView* view;

// Reload any displayed data to ensure the view is up to date.
- (void)reload;

// Notifies the NewTabPagePanelProtocol that it has been shown.
- (void)wasShown;

// Notifies the NewTabPagePanelProtocol that it has been hidden.
- (void)wasHidden;

// Dismisses any modal interaction elements.
- (void)dismissModals;

// Dismisses on-screen keyboard if necessary.
- (void)dismissKeyboard;

// Disable and enable scrollToTop
- (void)setScrollsToTop:(BOOL)enable;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_PANEL_PROTOCOL_H_
