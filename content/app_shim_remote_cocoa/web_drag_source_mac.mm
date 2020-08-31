// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/app_shim_remote_cocoa/web_drag_source_mac.h"

#include <sys/param.h>

#include <utility>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/mac/foundation_util.h"
#include "base/pickle.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/download/drag_download_file.h"
#include "content/browser/download/drag_download_util.h"
#include "content/common/web_contents_ns_view_bridge.mojom.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/drop_data.h"
#include "net/base/escape.h"
#include "net/base/filename_util.h"
#include "net/base/mime_util.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_util_mac.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/cocoa/cocoa_base_utils.h"
#include "ui/base/dragdrop/cocoa_dnd_util.h"
#include "ui/gfx/image/image.h"
#include "ui/resources/grit/ui_resources.h"
#include "url/url_constants.h"

using base::SysNSStringToUTF8;
using base::SysUTF8ToNSString;
using base::SysUTF16ToNSString;
using content::DropData;

@interface WebDragSource(Private)

- (void)fillPasteboard;
- (NSImage*)dragImage;

@end  // @interface WebDragSource(Private)


@implementation WebDragSource

- (id)initWithHost:(remote_cocoa::mojom::WebContentsNSViewHost*)host
              view:(NSView*)contentsView
          dropData:(const DropData*)dropData
             image:(NSImage*)image
            offset:(NSPoint)offset
        pasteboard:(NSPasteboard*)pboard
 dragOperationMask:(NSDragOperation)dragOperationMask {
  if ((self = [super init])) {
    _host = host;

    _contentsView = contentsView;
    DCHECK(_contentsView);

    _dropData.reset(new DropData(*dropData));
    DCHECK(_dropData.get());

    _dragImage.reset([image retain]);
    _imageOffset = offset;

    _pasteboard.reset([pboard retain]);
    DCHECK(_pasteboard.get());

    _dragOperationMask = dragOperationMask;

    [self fillPasteboard];
  }

  return self;
}

- (void)clearHostAndWebContentsView {
  _host = nullptr;
  _contentsView = nil;
}

- (NSDragOperation)draggingSourceOperationMaskForLocal:(BOOL)isLocal {
  return _dragOperationMask;
}

- (void)lazyWriteToPasteboard:(NSPasteboard*)pboard forType:(NSString*)type {
  // NSHTMLPboardType requires the character set to be declared. Otherwise, it
  // assumes US-ASCII. Awesome.
  const base::string16 kHtmlHeader = base::ASCIIToUTF16(
      "<meta http-equiv=\"Content-Type\" content=\"text/html;charset=UTF-8\">");

  // Be extra paranoid; avoid crashing.
  if (!_dropData) {
    NOTREACHED();
    return;
  }

  // HTML.
  if ([type isEqualToString:NSHTMLPboardType] ||
      [type isEqualToString:ui::kChromeDragImageHTMLPboardType]) {
    DCHECK(!_dropData->html.string().empty());
    // See comment on |kHtmlHeader| above.
    [pboard setString:SysUTF16ToNSString(kHtmlHeader + _dropData->html.string())
              forType:type];

  // URL.
  } else if ([type isEqualToString:NSURLPboardType] ||
             [type isEqualToString:base::mac::CFToNSCast(kUTTypeURL)]) {
    DCHECK(_dropData->url.is_valid());
    NSURL* url = [NSURL URLWithString:SysUTF8ToNSString(_dropData->url.spec())];
    // If NSURL creation failed, check for a badly-escaped JavaScript URL.
    // Strip out any existing escapes and then re-escape uniformly.
    if (!url && _dropData->url.SchemeIs(url::kJavaScriptScheme)) {
      std::string unescapedUrlString =
          net::UnescapeBinaryURLComponent(_dropData->url.spec());
      std::string escapedUrlString =
          net::EscapeUrlEncodedData(unescapedUrlString, false);
      url = [NSURL URLWithString:SysUTF8ToNSString(escapedUrlString)];
    }
    [url writeToPasteboard:pboard];
  // URL title.
  } else if ([type isEqualToString:ui::kUTTypeURLName]) {
    [pboard setString:SysUTF16ToNSString(_dropData->url_title)
              forType:ui::kUTTypeURLName];

  // File contents.
  } else if ([type isEqualToString:base::mac::CFToNSCast(_fileUTI)]) {
    [pboard setData:[NSData dataWithBytes:_dropData->file_contents.data()
                                   length:_dropData->file_contents.length()]
            forType:base::mac::CFToNSCast(_fileUTI.get())];

  // Plain text.
  } else if ([type isEqualToString:NSStringPboardType]) {
    DCHECK(!_dropData->text.string().empty());
    [pboard setString:SysUTF16ToNSString(_dropData->text.string())
              forType:NSStringPboardType];

  // Custom MIME data.
  } else if ([type isEqualToString:ui::kWebCustomDataPboardType]) {
    base::Pickle pickle;
    ui::WriteCustomDataToPickle(_dropData->custom_data, &pickle);
    [pboard setData:[NSData dataWithBytes:pickle.data() length:pickle.size()]
            forType:ui::kWebCustomDataPboardType];

  // Dummy type.
  } else if ([type isEqualToString:ui::kChromeDragDummyPboardType]) {
    // The dummy type _was_ promised and someone decided to call the bluff.
    [pboard setData:[NSData data]
            forType:ui::kChromeDragDummyPboardType];

  // Oops!
  } else {
    // Unknown drag pasteboard type.
    NOTREACHED();
  }
}

- (NSPoint)convertScreenPoint:(NSPoint)screenPoint {
  DCHECK([_contentsView window]);
  NSPoint basePoint =
      ui::ConvertPointFromScreenToWindow([_contentsView window], screenPoint);
  return [_contentsView convertPoint:basePoint fromView:nil];
}

- (void)startDrag {
  if (!_contentsView)
    return;

  NSEvent* currentEvent = [NSApp currentEvent];

  // Synthesize an event for dragging, since we can't be sure that
  // [NSApp currentEvent] will return a valid dragging event.
  NSWindow* window = [_contentsView window];
  NSPoint position = [window mouseLocationOutsideOfEventStream];
  NSTimeInterval eventTime = [currentEvent timestamp];
  NSEvent* dragEvent = [NSEvent mouseEventWithType:NSLeftMouseDragged
                                          location:position
                                     modifierFlags:NSLeftMouseDraggedMask
                                         timestamp:eventTime
                                      windowNumber:[window windowNumber]
                                           context:nil
                                       eventNumber:0
                                        clickCount:1
                                          pressure:1.0];

  if (_dragImage) {
    position.x -= _imageOffset.x;
    // Deal with Cocoa's flipped coordinate system.
    position.y -= [_dragImage.get() size].height - _imageOffset.y;
  }
  // Per kwebster, offset arg is ignored, see -_web_DragImageForElement: in
  // third_party/WebKit/Source/WebKit/mac/Misc/WebNSViewExtras.m.
  [window dragImage:[self dragImage]
                 at:position
             offset:NSZeroSize
              event:dragEvent
         pasteboard:_pasteboard
             source:_contentsView
          slideBack:YES];
}

- (void)endDragAt:(NSPoint)screenPoint
        operation:(NSDragOperation)operation {
  if (!_host || !_contentsView)
    return;

  if (_dragImage) {
    screenPoint.x += _imageOffset.x;
    // Deal with Cocoa's flipped coordinate system.
    screenPoint.y += [_dragImage.get() size].height - _imageOffset.y;
  }

  // Convert |screenPoint| to view coordinates and flip it.
  NSPoint localPoint = NSZeroPoint;
  if ([_contentsView window])
    localPoint = [self convertScreenPoint:screenPoint];
  NSRect viewFrame = [_contentsView frame];
  // Flip |screenPoint|.
  NSRect screenFrame = [[[_contentsView window] screen] frame];

  // If AppKit returns a copy and move operation, mask off the move bit
  // because WebCore does not understand what it means to do both, which
  // results in an assertion failure/renderer crash.
  if (operation == (NSDragOperationMove | NSDragOperationCopy))
    operation &= ~NSDragOperationMove;

  _host->EndDrag(
      operation,
      gfx::PointF(localPoint.x, viewFrame.size.height - localPoint.y),
      gfx::PointF(screenPoint.x, screenFrame.size.height - screenPoint.y));

  // Make sure the pasteboard owner isn't us.
  [_pasteboard declareTypes:[NSArray array] owner:nil];
}

- (NSString*)dragPromisedFileTo:(NSString*)path {
  if (!_host)
    return nil;
  // Be extra paranoid; avoid crashing.
  if (!_dropData) {
    NOTREACHED() << "No drag-and-drop data available for promised file.";
    return nil;
  }
  base::FilePath filePath(SysNSStringToUTF8(path));
  filePath = filePath.Append(_downloadFileName);
  _host->DragPromisedFileTo(filePath, *_dropData, _downloadURL, &filePath);
  return SysUTF8ToNSString(filePath.BaseName().value());
}

@end  // @implementation WebDragSource

@implementation WebDragSource (Private)

- (void)fillPasteboard {
  if (!_contentsView)
    return;

  DCHECK(_pasteboard.get());

  [_pasteboard declareTypes:@[ ui::kChromeDragDummyPboardType ]
                      owner:_contentsView];

  // URL (and title).
  if (_dropData->url.is_valid()) {
    [_pasteboard addTypes:@[
      NSURLPboardType, ui::kUTTypeURLName, base::mac::CFToNSCast(kUTTypeURL)
    ]
                    owner:_contentsView];
  }

  // MIME type.
  std::string mimeType;

  // File.
  if (!_dropData->file_contents.empty() ||
      !_dropData->download_metadata.empty()) {
    // TODO(https://crbug.com/898608): The |downloadFileName_| and
    // |downloadURL_| values should be computed by the caller.
    if (_dropData->download_metadata.empty()) {
      base::Optional<base::FilePath> suggestedFilename =
          _dropData->GetSafeFilenameForImageFileContents();
      if (suggestedFilename) {
        _downloadFileName = std::move(*suggestedFilename);
        net::GetMimeTypeFromFile(_downloadFileName, &mimeType);
      }
    } else {
      base::string16 mimeType16;
      base::FilePath fileName;
      if (content::ParseDownloadMetadata(
              _dropData->download_metadata,
              &mimeType16,
              &fileName,
              &_downloadURL)) {
        // Generate the file name based on both mime type and proposed file
        // name.
        std::string defaultName =
            content::GetContentClient()->browser()->GetDefaultDownloadName();
        mimeType = base::UTF16ToUTF8(mimeType16);
        _downloadFileName =
            net::GenerateFileName(_downloadURL,
                                  std::string(),
                                  std::string(),
                                  fileName.value(),
                                  mimeType,
                                  defaultName);
      }
    }

    if (!mimeType.empty()) {
      base::ScopedCFTypeRef<CFStringRef> mimeTypeCF(
          base::SysUTF8ToCFStringRef(mimeType));
      _fileUTI.reset(UTTypeCreatePreferredIdentifierForTag(
          kUTTagClassMIMEType, mimeTypeCF.get(), NULL));

      // File (HFS) promise.
      // There are two ways to drag/drop files. NSFilesPromisePboardType is the
      // deprecated way, and kPasteboardTypeFilePromiseContent is the way that
      // does not work. kPasteboardTypeFilePromiseContent is thoroughly broken:
      // * API: There is no good way to get the location for the drop.
      //   <http://lists.apple.com/archives/cocoa-dev/2012/Feb/msg00706.html>
      //   <rdar://14943849> <http://openradar.me/14943849>
      // * Behavior: A file dropped in the Finder is not selected. This can be
      //   worked around by selecting the file in the Finder using AppleEvents,
      //   but the drop target window will come to the front of the Finder's
      //   window list (unlike the previous behavior). <http://crbug.com/278515>
      //   <rdar://14943865> <http://openradar.me/14943865>
      // * Behavior: Files dragged over app icons in the dock do not highlight
      //   the dock icons, and the dock icons do not accept the drop.
      //   <http://crbug.com/282916> <rdar://14943872>
      //   <http://openradar.me/14943872>
      // * Behavior: A file dropped onto the desktop is positioned at the upper
      //   right of the desktop rather than at the position at which it was
      //   dropped. <http://crbug.com/284942> <rdar://14943881>
      //   <http://openradar.me/14943881>
      NSArray* fileUTIList = @[ base::mac::CFToNSCast(_fileUTI.get()) ];
      [_pasteboard addTypes:@[ NSFilesPromisePboardType ] owner:_contentsView];
      [_pasteboard setPropertyList:fileUTIList
                           forType:NSFilesPromisePboardType];

      if (!_dropData->file_contents.empty())
        [_pasteboard addTypes:fileUTIList owner:_contentsView];
    }
  }

  // HTML.
  bool hasHTMLData = !_dropData->html.string().empty();
  // Mail.app and TextEdit accept drags that have both HTML and image flavors on
  // them, but don't process them correctly <http://crbug.com/55879>. Therefore,
  // if there is an image flavor, don't put the HTML data on as HTML, but rather
  // put it on as this Chrome-only flavor.
  //
  // (The only time that Blink fills in the DropData::file_contents is with
  // an image drop, but the MIME time is tested anyway for paranoia's sake.)
  bool hasImageData = !_dropData->file_contents.empty() &&
                      _fileUTI &&
                      UTTypeConformsTo(_fileUTI.get(), kUTTypeImage);
  if (hasHTMLData) {
    if (hasImageData) {
      [_pasteboard addTypes:@[ ui::kChromeDragImageHTMLPboardType ]
                      owner:_contentsView];
    } else {
      [_pasteboard addTypes:@[ NSHTMLPboardType ] owner:_contentsView];
    }
  }

  // Plain text.
  if (!_dropData->text.string().empty()) {
    [_pasteboard addTypes:@[ NSStringPboardType ]
                    owner:_contentsView];
  }

  if (!_dropData->custom_data.empty()) {
    [_pasteboard addTypes:@[ ui::kWebCustomDataPboardType ]
                    owner:_contentsView];
  }
}

- (NSImage*)dragImage {
  if (_dragImage)
    return _dragImage;

  // Default to returning a generic image.
  return content::GetContentClient()->GetNativeImageNamed(
      IDR_DEFAULT_FAVICON).ToNSImage();
}

@end  // @implementation WebDragSource (Private)
