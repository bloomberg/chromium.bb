// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/print/print_controller.h"

#import <MobileCoreServices/UTType.h>
#import <Webkit/Webkit.h>

#include <memory>

#include "base/callback_helpers.h"
#import "base/ios/ios_util.h"
#import "base/ios/weak_nsobject.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/mac/bind_objc_block.h"
#include "base/mac/foundation_util.h"
#import "base/mac/scoped_nsobject.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#import "ios/chrome/browser/ui/alert_coordinator/loading_alert_coordinator.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/web/public/web_thread.h"
#import "net/base/mac/url_conversions.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/l10n/l10n_util_mac.h"

using net::URLFetcher;
using net::URLFetcherDelegate;
using net::URLRequestContextGetter;

@interface PrintController ()

// Presents a UIPrintInteractionController with a default completion handler.
// |isPDF| indicates if |printInteractionController| is being presented to print
// a PDF.
+ (void)displayPrintInteractionController:
            (UIPrintInteractionController*)printInteractionController
                                   forPDF:(BOOL)isPDF;

// Shows a dialog on |viewController| indicating that the print preview is being
// prepared. The dialog will appear only if the download has not completed or
// been cancelled |kPDFDownloadDialogDelay| seconds after this method is called.
- (void)showPDFDownloadingDialog:(UIViewController*)viewController;

// Dismisses the dialog which indicates that the print preview is being
// prepared.
- (void)dismissPDFDownloadingDialog;

// Shows an dialog on |viewController| indicating that there was an error when
// preparing the print preview, and providing the ability to retry. |URL| is the
// URL of the PDF which had an error.
- (void)showPDFDownloadErrorWithURL:(const GURL)URL
                     viewController:(UIViewController*)viewController;

// Handles downloading the file at |URL| and presents dialogs on
// |viewController| if necessary.
- (void)downloadPDFFileWithURL:(const GURL&)URL
                viewController:(UIViewController*)viewController;

// Accesses the result of the PDF download, and if successful, presents the
// AirPrint menu with the downloaded PDF. It also ensures that the
// PDFDownloadingDialog is dismissed, and presents an error dialog on
// |viewController| if the download fails. This method should be called only by
// the URLFetcherDelegate.
- (void)finishedDownloadingPDF:(UIViewController*)viewController;

@end

namespace {

// The MIME type of a PDF file contained in a Web view.
const char kPDFMimeType[] = "application/pdf";

// Delay after downloading begins that the |_PDFDownloadingDialog| appears.
const int64_t kPDFDownloadDialogDelay = 1;

// A delegate for the URLFetcher to tell owning PrintController that the
// download is complete.
class PrintPDFFetcherDelegate : public URLFetcherDelegate {
 public:
  explicit PrintPDFFetcherDelegate(PrintController* owner) : owner_(owner) {}
  void OnURLFetchComplete(const URLFetcher* source) override {
    DCHECK(view_controller_);
    [owner_ finishedDownloadingPDF:view_controller_];
  }

  // The ViewController used to display an error if the download failed.
  void SetViewController(UIViewController* view_controller) {
    view_controller_ = view_controller;
  }

 private:
  __unsafe_unretained PrintController* owner_;
  __unsafe_unretained UIViewController* view_controller_;
  DISALLOW_COPY_AND_ASSIGN(PrintPDFFetcherDelegate);
};
}  // namespace

@implementation PrintController {
  // URLFetcher to download the PDF pointed to by the WKWebView.
  std::unique_ptr<URLFetcher> _fetcher;

  // A delegate to bridge between PrintController and the URLFetcher callback.
  std::unique_ptr<PrintPDFFetcherDelegate> _fetcherDelegate;

  // Context getter required by the URLFetcher.
  scoped_refptr<URLRequestContextGetter> _requestContextGetter;

  // A dialog which indicates that the print preview is being prepared. It
  // offers a cancel button which will cancel the download. It is created when
  // downloading begins and is released when downloading ends (either due to
  // cancellation or completion).
  base::scoped_nsobject<LoadingAlertCoordinator> _PDFDownloadingDialog;

  // A dialog which indicates that the print preview failed.
  base::scoped_nsobject<AlertCoordinator> _PDFDownloadingErrorDialog;
}

#pragma mark - Class methods.

+ (void)displayPrintInteractionController:
            (UIPrintInteractionController*)printInteractionController
                                   forPDF:(BOOL)isPDF {
  void (^completionHandler)(UIPrintInteractionController*, BOOL, NSError*) = ^(
      UIPrintInteractionController* printInteractionController, BOOL completed,
      NSError* error) {
    if (error)
      DLOG(ERROR) << "Air printing error: " << error.description;

    // When printing a NSData object given to the
    // UIPrintInteractionController's |printingItem| object, a PDF file
    // representing the NSData object is created in the app's tmp directory
    // by the OS and never deleted. So, this workaround deletes PDF files in
    // tmp now that printing is done. When iOS9 is deprecated, this can
    // be removed since PDFs will no longer need to be downloaded to print,
    // and |printingItem| will no longer be used.
    if (!base::ios::IsRunningOnIOS10OrLater() && isPDF) {
      web::WebThread::PostBlockingPoolTask(
          FROM_HERE, base::BindBlock(^{
            NSFileManager* manager = [NSFileManager defaultManager];
            NSString* tempDir = NSTemporaryDirectory();
            NSError* tempDirError = nil;

            // Iterate over files in tmp directory.
            for (NSString* file in
                 [manager contentsOfDirectoryAtPath:tempDir
                                              error:&tempDirError]) {
              // If the file is a PDF file, delete it.
              if ([[file pathExtension] isEqualToString:@"pdf"]) {
                NSError* deletionError = nil;
                NSString* fullFilePath =
                    [tempDir stringByAppendingPathComponent:file];
                BOOL success = [manager removeItemAtPath:fullFilePath
                                                   error:&deletionError];
                if (!success) {
                  DLOG(ERROR) << "AirPrint unable to remove tmp file:" << file
                              << " error: " << deletionError.description;
                }
              }
            }
            if (tempDirError) {
              DLOG(ERROR) << "AirPrint tmp dir access error:"
                          << tempDirError.description;
            }
          }));
    }
  };
  [printInteractionController presentAnimated:YES
                            completionHandler:completionHandler];
}

#pragma mark - Public Methods

- (instancetype)initWithContextGetter:
    (scoped_refptr<net::URLRequestContextGetter>)getter {
  self = [super init];
  if (self) {
    _requestContextGetter = std::move(getter);
    _fetcherDelegate.reset(new PrintPDFFetcherDelegate(self));
  }
  return self;
}

- (instancetype)init {
  NOTREACHED();
  return nil;
}

- (void)printView:(UIView*)view
         withTitle:(NSString*)title
    viewController:(UIViewController*)viewController {
  base::RecordAction(base::UserMetricsAction("MobilePrintMenuAirPrint"));
  UIPrintInteractionController* printInteractionController =
      [UIPrintInteractionController sharedPrintController];
  UIPrintInfo* printInfo = [UIPrintInfo printInfo];
  printInfo.outputType = UIPrintInfoOutputGeneral;
  printInfo.jobName = title;
  printInteractionController.printInfo = printInfo;
  printInteractionController.showsPageRange = YES;

  // Print Formatters do not work for PDFs in iOS9 WKWebView, but do in iOS10.
  // Instead, download the PDF and (eventually) pass it to the
  // UIPrintInteractionController. Remove this workaround and all associated PDF
  // specific code when iOS9 is deprecated.
  BOOL isPDFURL = NO;
  if (!base::ios::IsRunningOnIOS10OrLater() &&
      [view isMemberOfClass:[WKWebView class]]) {
    WKWebView* webView = base::mac::ObjCCastStrict<WKWebView>(view);
    NSURL* URL = webView.URL;
    CFStringRef UTI = UTTypeCreatePreferredIdentifierForTag(
        kUTTagClassFilenameExtension,
        (__bridge CFStringRef)[[URL path] pathExtension], NULL);
    if (UTI) {
      CFStringRef MIMEType =
          UTTypeCopyPreferredTagWithClass(UTI, kUTTagClassMIMEType);
      if (MIMEType) {
        isPDFURL =
            [@(kPDFMimeType) isEqualToString:(__bridge NSString*)MIMEType];
        if (isPDFURL) {
          [self downloadPDFFileWithURL:net::GURLWithNSURL(URL)
                        viewController:viewController];
        }
        CFRelease(MIMEType);
      }
      CFRelease(UTI);
    }
  }

  if (!isPDFURL) {
    base::scoped_nsobject<UIPrintPageRenderer> renderer(
        [[UIPrintPageRenderer alloc] init]);
    [renderer addPrintFormatter:[view viewPrintFormatter]
          startingAtPageAtIndex:0];
    printInteractionController.printPageRenderer = renderer;
    [PrintController
        displayPrintInteractionController:printInteractionController
                                   forPDF:NO];
  }
}

- (void)dismissAnimated:(BOOL)animated {
  _fetcher.reset();
  [self dismissPDFDownloadingDialog];
  [_PDFDownloadingErrorDialog stop];
  _PDFDownloadingErrorDialog.reset();
  [[UIPrintInteractionController sharedPrintController]
      dismissAnimated:animated];
}

#pragma mark - Private Methods

- (void)showPDFDownloadingDialog:(UIViewController*)viewController {
  if (_PDFDownloadingDialog)
    return;

  NSString* title = l10n_util::GetNSString(IDS_IOS_PRINT_PDF_PREPARATION);

  base::WeakNSObject<PrintController> weakSelf(self);
  ProceduralBlock cancelHandler = ^{
    base::scoped_nsobject<PrintController> strongSelf([weakSelf retain]);
    if (strongSelf)
      strongSelf.get()->_fetcher.reset();
  };

  _PDFDownloadingDialog.reset([[LoadingAlertCoordinator alloc]
      initWithBaseViewController:viewController
                           title:title
                   cancelHandler:cancelHandler]);

  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW, kPDFDownloadDialogDelay * NSEC_PER_SEC),
      dispatch_get_main_queue(), ^{
        base::scoped_nsobject<PrintController> strongSelf([weakSelf retain]);
        if (!strongSelf)
          return;
        [strongSelf.get()->_PDFDownloadingDialog start];
      });
}

- (void)dismissPDFDownloadingDialog {
  [_PDFDownloadingDialog stop];
  _PDFDownloadingDialog.reset();
}

- (void)showPDFDownloadErrorWithURL:(const GURL)URL
                     viewController:(UIViewController*)viewController {
  NSString* title = l10n_util::GetNSString(IDS_IOS_PRINT_PDF_ERROR_TITLE);
  NSString* message = l10n_util::GetNSString(IDS_IOS_PRINT_PDF_ERROR_SUBTITLE);

  _PDFDownloadingErrorDialog.reset([[AlertCoordinator alloc]
      initWithBaseViewController:viewController
                           title:title
                         message:message]);

  base::WeakNSObject<PrintController> weakSelf(self);

  [_PDFDownloadingErrorDialog
      addItemWithTitle:l10n_util::GetNSString(IDS_IOS_PRINT_PDF_TRY_AGAIN)
                action:^{
                  [weakSelf downloadPDFFileWithURL:URL
                                    viewController:viewController];
                }
                 style:UIAlertActionStyleDefault];

  [_PDFDownloadingErrorDialog
      addItemWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                action:nil
                 style:UIAlertActionStyleCancel];

  [_PDFDownloadingErrorDialog start];
}

- (void)downloadPDFFileWithURL:(const GURL&)URL
                viewController:(UIViewController*)viewController {
  DCHECK(!_fetcher);
  _fetcherDelegate->SetViewController(viewController);
  _fetcher = URLFetcher::Create(URL, URLFetcher::GET, _fetcherDelegate.get());
  _fetcher->SetRequestContext(_requestContextGetter.get());
  _fetcher->Start();
  [self showPDFDownloadingDialog:viewController];
}

- (void)finishedDownloadingPDF:(UIViewController*)viewController {
  [self dismissPDFDownloadingDialog];
  DCHECK(_fetcher);
  base::ScopedClosureRunner fetcherResetter(base::BindBlock(^{
    _fetcher.reset();
  }));
  int responseCode = _fetcher->GetResponseCode();
  std::string response;
  std::string MIMEType;
  // If the request is not successful or does not match a PDF
  // MIME type, show an error.
  if (!_fetcher->GetStatus().is_success() || responseCode != 200 ||
      !_fetcher->GetResponseAsString(&response) ||
      !_fetcher->GetResponseHeaders()->GetMimeType(&MIMEType) ||
      MIMEType != kPDFMimeType) {
    [self showPDFDownloadErrorWithURL:_fetcher->GetOriginalURL()
                       viewController:viewController];
    return;
  }
  NSData* data =
      [NSData dataWithBytes:response.c_str() length:response.length()];
  // If the data cannot be printed, show an error.
  if (![UIPrintInteractionController canPrintData:data]) {
    [self showPDFDownloadErrorWithURL:_fetcher->GetOriginalURL()
                       viewController:viewController];
    return;
  }
  UIPrintInteractionController* printInteractionController =
      [UIPrintInteractionController sharedPrintController];
  printInteractionController.printingItem = data;
  [PrintController displayPrintInteractionController:printInteractionController
                                              forPDF:YES];
}

@end
