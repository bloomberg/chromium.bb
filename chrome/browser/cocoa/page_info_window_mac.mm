// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/page_info_window_mac.h"

#include <algorithm>

#include "app/l10n_util.h"
#include "base/scoped_cftyperef.h"
#include "app/resource_bundle.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#import "chrome/browser/cocoa/page_info_window_controller.h"
#include "chrome/browser/cert_store.h"
#include "chrome/browser/certificate_viewer.h"
#include "chrome/browser/page_info_window.h"
#include "chrome/browser/profile.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/base/cert_status_flags.h"
#include "net/base/x509_certificate.h"
#import "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"

namespace {

// Spacing in between section boxes.
const NSInteger kVerticalSpacing = 10;

// Padding from the edge of the boxes and the window frame.
const NSInteger kFramePadding = 20;

// Spacing between the frame of the box and the left edge of the image and the
// right edge of the text.
const NSInteger kBoxHorizontalSpacing = 13;

// Spacing between the top/bottom of the box frame and the top of the image and
// text components.
const NSInteger kBoxTopSpacing = 33;
const NSInteger kBoxBottomSpacing = 7;

// Spacing between the image and the text.
const NSInteger kImageSpacing = 10;

// Square size of the image.
const CGFloat kImageSize = 30;

}  // namespace

namespace browser {

void ShowPageInfo(gfx::NativeWindow parent,
                  Profile* profile,
                  const GURL& url,
                  const NavigationEntry::SSLStatus& ssl,
                  bool show_history) {
  PageInfoWindowMac::ShowPageInfo(parent, profile, url, ssl, show_history);
}

}  // namespace browser

PageInfoWindowMac::PageInfoWindowMac(PageInfoWindowController* controller,
                                     Profile* profile,
                                     const GURL& url,
                                     const NavigationEntry::SSLStatus& ssl,
                                     bool show_history)
    : controller_(controller),
      model_(new PageInfoModel(profile, url, ssl, show_history, this)),
      cert_id_(ssl.cert_id()) {
  Init();
}

PageInfoWindowMac::PageInfoWindowMac(PageInfoWindowController* controller,
                                     PageInfoModel* model)
    : controller_(controller),
      model_(model),
      cert_id_(0) {
  Init();
}

// static
void PageInfoWindowMac::ShowPageInfo(gfx::NativeWindow parent,
                                     Profile* profile,
                                     const GURL& url,
                                     const NavigationEntry::SSLStatus& ssl,
                                     bool show_history) {
  // The controller will clean itself up after the NSWindow it manages closes.
  // We do not manage it as it owns us.
  PageInfoWindowController* controller =
      [[PageInfoWindowController alloc] init];
  PageInfoWindowMac* page_info = new PageInfoWindowMac(controller,
                                                       profile,
                                                       url,
                                                       ssl,
                                                       show_history);
  [controller setPageInfo:page_info];
  page_info->LayoutSections();
  page_info->Show();
}

void PageInfoWindowMac::Init() {
  // Load the image refs.
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  good_image_.reset([rb.GetNSImageNamed(IDR_PAGEINFO_GOOD) retain]);
  DCHECK_GE(kImageSize, [good_image_ size].width);
  DCHECK_GE(kImageSize, [good_image_ size].height);
  bad_image_.reset([rb.GetNSImageNamed(IDR_PAGEINFO_BAD) retain]);
  DCHECK_GE(kImageSize, [bad_image_ size].width);
  DCHECK_GE(kImageSize, [bad_image_ size].height);
}

PageInfoWindowMac::~PageInfoWindowMac() {
}

void PageInfoWindowMac::ShowCertDialog(int) {
  DCHECK(cert_id_ != 0);
  ShowCertificateViewerByID([controller_ window], cert_id_);
}

void PageInfoWindowMac::ModelChanged() {
  LayoutSections();
}

void PageInfoWindowMac::Show() {
  [[controller_ window] makeKeyAndOrderFront:nil];
}

// This will create the subviews for the page info window. The general layout
// is 2 or 3 boxed and titled sections, each of which has a status image to
// provide visual feedback and a description that explains it. The description
// text is usually only 1 or 2 lines, but can be much longer. At the bottom of
// the window is a button to view the SSL certificate, which is disabled if
// not using HTTPS.
void PageInfoWindowMac::LayoutSections() {
  NSRect window_frame = [[controller_ window] frame];
  CGFloat window_width = NSWidth(window_frame);
  // |offset| is the Y position that should be drawn at next.
  CGFloat offset = kFramePadding;

  // Keep the new subviews in an array that gets replaced at the end.
  NSMutableArray* subviews = [NSMutableArray array];

  // Create the certificate button. The frame will be fixed up by GTM, so use
  // arbitrary values.
  NSRect button_rect = NSMakeRect(kFramePadding, offset, 100, 20);
  scoped_nsobject<NSButton> cert_button(
      [[NSButton alloc] initWithFrame:button_rect]);
  [cert_button setTitle:
      l10n_util::GetNSStringWithFixup(IDS_PAGEINFO_CERT_INFO_BUTTON)];
  [cert_button setButtonType:NSMomentaryPushInButton];
  [cert_button setBezelStyle:NSRoundedBezelStyle];
  // The default button will use control size font, not the system/button font.
  [cert_button setFont:[NSFont systemFontOfSize:0]];
  [cert_button setTarget:controller_];
  [cert_button setAction:@selector(showCertWindow:)];
  [subviews addObject:cert_button.get()];
  [GTMUILocalizerAndLayoutTweaker sizeToFitView:cert_button];
  offset += NSHeight(button_rect) + kFramePadding;

  // Small font for the description labels.
  NSFont* font = [NSFont labelFontOfSize:11];

  // Calculate some common, constant values.
  const CGFloat box_width = window_width - (2 * kFramePadding);
  const CGFloat text_width =
      box_width - (kBoxHorizontalSpacing + kImageSize +
          kImageSpacing + kBoxHorizontalSpacing * 2);
  const CGFloat text_x = kBoxHorizontalSpacing + kImageSize + kImageSpacing;

  // Build the window from bottom-up because Cocoa's coordinate origin is the
  // lower left.
  for (int i = model_->GetSectionCount() - 1; i >= 0; --i) {
    PageInfoModel::SectionInfo info = model_->GetSectionInfo(i);

    // Create the text field first, because that informs how large to make the
    // box it goes in.
    NSRect text_field_rect =
        NSMakeRect(text_x, kBoxBottomSpacing, text_width, kImageSpacing);
    scoped_nsobject<NSTextField> text_field(
        [[NSTextField alloc] initWithFrame:text_field_rect]);
    [text_field setEditable:NO];
    [text_field setDrawsBackground:NO];
    [text_field setBezeled:NO];
    [text_field setStringValue:base::SysUTF16ToNSString(info.description)];
    [text_field setFont:font];

    // If the text is oversized, resize the text field.
    text_field_rect.size.height +=
        [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:
            text_field];

    // Make the box, sized to fit the text. It should be at least large enough
    // to contain the height of the image, which is larger than a single line
    // of text.
    CGFloat box_height = kBoxTopSpacing +
        std::max(NSHeight(text_field_rect), kImageSize) + kBoxBottomSpacing;
    NSRect box_rect = NSMakeRect(kFramePadding, offset, box_width, box_height);
    scoped_nsobject<NSBox> box([[NSBox alloc] initWithFrame:box_rect]);
    [box setTitlePosition:NSAtTop];
    [box setTitle:base::SysUTF16ToNSString(info.title)];
    offset += box_height + kVerticalSpacing;

    // Re-position the text field's origin now that the box height is known.
    text_field_rect.origin.y = box_height -
        (NSHeight(text_field_rect) + kBoxTopSpacing);
    [text_field setFrame:text_field_rect];

    // Insert the image subview.
    NSRect image_view_rect =
        NSMakeRect(kBoxHorizontalSpacing,
                   box_height - (kImageSize + kBoxTopSpacing),
                   kImageSize, kImageSize);
    scoped_nsobject<NSImageView> image_view(
        [[NSImageView alloc] initWithFrame:image_view_rect]);
    [image_view setImageFrameStyle:NSImageFrameNone];
    [image_view setImage:(info.state == PageInfoModel::SECTION_STATE_OK) ?
        good_image_.get() : bad_image_.get()];

    // Add the box to the list of new subviews.
    [box addSubview:image_view.get()];
    [box addSubview:text_field.get()];
    [subviews addObject:box.get()];
  }

  // Replace the window's content.
  [[[controller_ window] contentView] setSubviews:subviews];

  // Just setting |size| will cause the window to grow upwards. Shift the
  // origin up by grow amount, which causes the window to grow downwards.
  offset += kFramePadding;
  window_frame.origin.y -= offset - window_frame.size.height;
  window_frame.size.height = offset;

  // By default, assume that we don't have certificate information to show.
  [cert_button setEnabled:NO];
  if (cert_id_) {
    scoped_refptr<net::X509Certificate> cert;
    CertStore::GetSharedInstance()->RetrieveCert(cert_id_, &cert);

    // Don't bother showing certificates if there isn't one. Gears runs with no
    // os root certificate.
    if (cert.get() && cert->os_cert_handle()) {
      [cert_button setEnabled:YES];
    }
  }

  // Resize the window. Only animate if the window is visible, otherwise it
  // could be "growing" while it's opening, looking awkward.
  [[controller_ window] setFrame:window_frame
                         display:YES
                         animate:[[controller_ window] isVisible]];
}
