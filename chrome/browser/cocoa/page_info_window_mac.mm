// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/page_info_window_mac.h"

#include <Security/Security.h>
#include <SecurityInterface/SFCertificatePanel.h>

#include "app/l10n_util.h"
#include "base/scoped_cftyperef.h"
#include "base/i18n/time_formatting.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#import "chrome/browser/cocoa/page_info_window_controller.h"
#include "chrome/browser/cert_store.h"
#include "chrome/browser/profile.h"
#include "grit/generated_resources.h"
#include "net/base/cert_status_flags.h"
#include "net/base/x509_certificate.h"

void PageInfoWindowMac::ShowPageInfo(Profile* profile,
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

PageInfoWindowMac::PageInfoWindowMac(PageInfoWindowController* controller,
                                     Profile* profile,
                                     const GURL& url,
                                     const NavigationEntry::SSLStatus& ssl,
                                     bool show_history)
    : controller_(controller),
      model_(profile, url, ssl, show_history, this),
      cert_id_(ssl.cert_id()) {
}

PageInfoWindowMac::~PageInfoWindowMac() {
}

void PageInfoWindowMac::Show() {
  [[controller_ window] makeKeyAndOrderFront:nil];
}

void PageInfoWindowMac::ShowCertDialog(int) {
  DCHECK(cert_id_ != 0);
  scoped_refptr<net::X509Certificate> cert;
  CertStore::GetSharedInstance()->RetrieveCert(cert_id_, &cert);
  if (!cert.get()) {
    // The certificate was not found. Could be that the renderer crashed before
    // we displayed the page info.
    return;
  }

  SecCertificateRef cert_mac = cert->os_cert_handle();
  if (!cert_mac)
    return;

  scoped_cftyperef<CFMutableArrayRef> certificates(
      CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks));
  if (!certificates.get()) {
    NOTREACHED();
    return;
  }
  CFArrayAppendValue(certificates, cert_mac);

  CFArrayRef ca_certs = cert->GetIntermediateCertificates();
  if (ca_certs) {
    // Server certificate must be first in the array; subsequent certificates
    // in the chain can be in any order.
    CFArrayAppendArray(certificates, ca_certs,
                       CFRangeMake(0, CFArrayGetCount(ca_certs)));
  }

  [[SFCertificatePanel sharedCertificatePanel]
      beginSheetForWindow:[controller_ window]
            modalDelegate:nil
           didEndSelector:NULL
              contextInfo:NULL
             certificates:reinterpret_cast<NSArray*>(certificates.get())
                showGroup:YES
  ];
}

void PageInfoWindowMac::LayoutSections() {
  // These wstring's will be converted to NSString's and passed to the
  // window controller when we're done figuring out what text should go in them.
  std::wstring identity_msg;
  std::wstring connection_msg;

  // Identity section
  PageInfoModel::SectionInfo identity_section =
      model_.GetSectionInfo(PageInfoModel::IDENTITY);
  if (identity_section.state)
    [controller_ setIdentityImg:[controller_ goodImg]];
  else
    [controller_ setIdentityImg:[controller_ badImg]];
  [controller_ setIdentityMsg:base::SysUTF16ToNSString(
      identity_section.description)];

  // Connection section.
  PageInfoModel::SectionInfo connection_section =
      model_.GetSectionInfo(PageInfoModel::CONNECTION);
  if (connection_section.state)
    [controller_ setConnectionImg:[controller_ goodImg]];
  else
    [controller_ setConnectionImg:[controller_ badImg]];
  [controller_ setConnectionMsg:
      base::SysUTF16ToNSString(connection_section.description)];

  if (model_.GetSectionCount() > 2) {
    // We have the history info.
    PageInfoModel::SectionInfo history_section =
        model_.GetSectionInfo(PageInfoModel::HISTORY);
    if (history_section.state)
       [controller_ setHistoryImg:[controller_ goodImg]];
    else
      [controller_ setHistoryImg:[controller_ badImg]];

    [controller_ setHistoryMsg:
        base::SysUTF16ToNSString(history_section.description)];
  }

  // By default, assume that we don't have certificate information to show.
  [controller_ setEnableCertButton:NO];
  if (cert_id_) {
    scoped_refptr<net::X509Certificate> cert;
    CertStore::GetSharedInstance()->RetrieveCert(cert_id_, &cert);

    // Don't bother showing certificates if there isn't one. Gears runs with no
    // os root certificate.
    if (cert.get() && cert->os_cert_handle()) {
      [controller_ setEnableCertButton:YES];
    }
  }
}

void PageInfoWindowMac::ModelChanged() {
  // We have history information, so show the box and extend the window frame.
  [controller_ setShowHistoryBox:YES];
  LayoutSections();
}

