// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/web_drag_source.h"

#include "base/file_util.h"
#include "base/nsimage_cache_mac.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "base/task.h"
#include "base/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view_mac.h"
#include "net/base/file_stream.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#import "third_party/mozilla/include/NSPasteboard+Utils.h"
#include "webkit/glue/webdropdata.h"

using base::SysNSStringToUTF8;
using base::SysUTF8ToNSString;
using base::SysUTF16ToNSString;
using net::FileStream;


namespace {

// Make a drag image from the drop data.
// TODO(viettrungluu): Move this somewhere more sensible.
NSImage* MakeDragImage(const WebDropData* drop_data) {
  // TODO(viettrungluu): Just a stub for now. Make it do something (see, e.g.,
  // WebKit/WebKit/mac/Misc/WebNSViewExtras.m: |-_web_DragImageForElement:...|).

  // Default to returning a generic image.
  return nsimage_cache::ImageNamed(@"nav.pdf");
}

// Returns a filename appropriate for the drop data (of form "FILENAME-seq.EXT"
// if seq > 0).
// TODO(viettrungluu): Refactor to make it common across platforms,
// and move it somewhere sensible.
FilePath GetFileNameFromDragData(
    const WebDropData& drop_data, unsigned seq) {
  // Images without ALT text will only have a file extension so we need to
  // synthesize one from the provided extension and URL.
  FilePath file_name([SysUTF16ToNSString(drop_data.file_description_filename)
          fileSystemRepresentation]);
  file_name = file_name.BaseName().RemoveExtension();

  if (file_name.empty()) {
    // Retrieve the name from the URL.
    file_name = net::GetSuggestedFilename(drop_data.url, "", "", "");
  }

  file_name = file_name.ReplaceExtension([SysUTF16ToNSString(
          drop_data.file_extension) fileSystemRepresentation]);

  if (seq > 0) {
    file_name =
        file_name.InsertBeforeExtension(std::string("-")+UintToString(seq));
  }

  return file_name;
}

// This class's sole task is to write out data for a promised file; the caller
// is responsible for opening the file.
class PromiseWriterTask : public Task {
 public:
  // Assumes ownership of file_stream.
  PromiseWriterTask(const WebDropData& drop_data,
                    FileStream* file_stream);
  virtual ~PromiseWriterTask();
  virtual void Run();

 private:
  WebDropData drop_data_;

  // This class takes ownership of file_stream_ and will close and delete it.
  scoped_ptr<FileStream> file_stream_;
};

// Takes the drop data and an open file stream (which it takes ownership of and
// will close and delete).
PromiseWriterTask::PromiseWriterTask(const WebDropData& drop_data,
                                     FileStream* file_stream) :
    drop_data_(drop_data) {
  file_stream_.reset(file_stream);
  DCHECK(file_stream_.get());
}

PromiseWriterTask::~PromiseWriterTask() {
  DCHECK(file_stream_.get());
  if (file_stream_.get())
    file_stream_->Close();
}

void PromiseWriterTask::Run() {
  CHECK(file_stream_.get());
  file_stream_->Write(drop_data_.file_contents.data(),
                      drop_data_.file_contents.length(),
                      NULL);

  // Let our destructor take care of business.
}

}  // namespace


@interface WebDragSource(Private)

- (void)fillPasteboard;
- (NSImage*)dragImage;

@end  // @interface WebDragSource(Private)


@implementation WebDragSource

- (id)initWithContentsView:(TabContentsViewCocoa*)contentsView
                  dropData:(const WebDropData*)dropData
                pasteboard:(NSPasteboard*)pboard
         dragOperationMask:(NSDragOperation)dragOperationMask {
  if ((self = [super init])) {
    contentsView_ = contentsView;
    DCHECK(contentsView_);

    dropData_.reset(new WebDropData(*dropData));
    DCHECK(dropData_.get());

    pasteboard_.reset([pboard retain]);
    DCHECK(pasteboard_.get());

    dragOperationMask_ = dragOperationMask;

    [self fillPasteboard];
  }

  return self;
}

- (NSDragOperation)draggingSourceOperationMaskForLocal:(BOOL)isLocal {
  return dragOperationMask_;
}

- (void)lazyWriteToPasteboard:(NSPasteboard*)pboard forType:(NSString*)type {
  // Be extra paranoid; avoid crashing.
  if (!dropData_.get()) {
    NOTREACHED() << "No drag-and-drop data available for lazy write.";
    return;
  }

  // HTML.
  if ([type isEqualToString:NSHTMLPboardType]) {
    DCHECK(!dropData_->text_html.empty());
    [pboard setString:SysUTF16ToNSString(dropData_->text_html)
              forType:NSHTMLPboardType];

  // URL.
  } else if ([type isEqualToString:NSURLPboardType]) {
    DCHECK(dropData_->url.is_valid());
    [pboard     setURLs:[NSArray
        arrayWithObject:SysUTF8ToNSString(dropData_->url.spec())]
             withTitles:[NSArray arrayWithObject:
                            SysUTF16ToNSString(dropData_->url_title)]];

  // File contents.
  } else if ([type isEqualToString:NSFileContentsPboardType] ||
      [type isEqualToString:NSCreateFileContentsPboardType(
              SysUTF16ToNSString(dropData_->file_extension))]) {
    // TODO(viettrungluu: find something which is known to accept
    // NSFileContentsPboardType to check that this actually works!
    scoped_nsobject<NSFileWrapper> file_wrapper(
        [[NSFileWrapper alloc] initRegularFileWithContents:[NSData
                dataWithBytes:dropData_->file_contents.data()
                       length:dropData_->file_contents.length()]]);
    [file_wrapper setPreferredFilename:SysUTF8ToNSString(
            GetFileNameFromDragData(*dropData_, 0).value())];
    [pboard writeFileWrapper:file_wrapper];

  // TIFF.
  } else if ([type isEqualToString:NSTIFFPboardType]) {
    // TODO(viettrungluu): This is a bit odd since we rely on Cocoa to render
    // our image into a TIFF. This is also suboptimal since this is all done
    // synchronously. I'm not sure there's much we can easily do about it.
    scoped_nsobject<NSImage> image(
        [[NSImage alloc] initWithData:[NSData
                dataWithBytes:dropData_->file_contents.data()
                       length:dropData_->file_contents.length()]]);
    [pboard setData:[image TIFFRepresentation] forType:NSTIFFPboardType];

  // Plain text.
  } else if ([type isEqualToString:NSStringPboardType]) {
    DCHECK(!dropData_->plain_text.empty());
    [pboard setString:SysUTF16ToNSString(dropData_->plain_text)
              forType:NSStringPboardType];

  // Oops!
  } else {
    NOTREACHED() << "Asked for a drag pasteboard type we didn't offer.";
  }
}

- (void)startDrag {
  NSEvent* currentEvent = [NSApp currentEvent];

  // Synthesize an event for dragging, since we can't be sure that
  // [NSApp currentEvent] will return a valid dragging event.
  NSWindow* window = [contentsView_ window];
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

  [contentsView_ dragImage:[self dragImage]
                        at:position
                    offset:NSZeroSize
                     event:dragEvent
                pasteboard:pasteboard_
                    source:contentsView_
                 slideBack:YES];
}

- (void)endDragAt:(NSPoint)screenPoint
        operation:(NSDragOperation)operation {
  RenderViewHost* rvh = [contentsView_ tabContents]->render_view_host();
  if (rvh) {
    rvh->DragSourceSystemDragEnded();

    // Convert |screenPoint| to view coordinates and flip it.
    NSPoint localPoint = [contentsView_ convertPointFromBase:screenPoint];
    NSRect viewFrame = [contentsView_ frame];
    localPoint.y = viewFrame.size.height - localPoint.y;
    // Flip |screenPoint|.
    NSRect screenFrame = [[[contentsView_ window] screen] frame];
    screenPoint.y = screenFrame.size.height - screenPoint.y;

    rvh->DragSourceEndedAt(localPoint.x, localPoint.y,
                           screenPoint.x, screenPoint.y,
                           static_cast<WebKit::WebDragOperation>(operation));
  }

  // Make sure the pasteboard owner isn't us.
  [pasteboard_ declareTypes:[NSArray array] owner:nil];
}

- (void)moveDragTo:(NSPoint)screenPoint {
  RenderViewHost* rvh = [contentsView_ tabContents]->render_view_host();
  if (rvh) {
    // Convert |screenPoint| to view coordinates and flip it.
    NSPoint localPoint = [contentsView_ convertPointFromBase:screenPoint];
    NSRect viewFrame = [contentsView_ frame];
    localPoint.y = viewFrame.size.height - localPoint.y;
    // Flip |screenPoint|.
    NSRect screenFrame = [[[contentsView_ window] screen] frame];
    screenPoint.y = screenFrame.size.height - screenPoint.y;

    rvh->DragSourceMovedTo(localPoint.x, localPoint.y,
                           screenPoint.x, screenPoint.y);
  }
}

- (NSString*)dragPromisedFileTo:(NSString*)path {
  // Be extra paranoid; avoid crashing.
  if (!dropData_.get()) {
    NOTREACHED() << "No drag-and-drop data available for promised file.";
    return nil;
  }

  FileStream* file_stream = new FileStream;
  DCHECK(file_stream);
  if (!file_stream)
    return nil;

  FilePath path_name(SysNSStringToUTF8(path));
  FilePath file_name;
  unsigned seq;
  const unsigned k_max_seq = 99;
  for (seq = 0; seq <= k_max_seq; seq++) {
    file_name = GetFileNameFromDragData(*dropData_, seq);
    FilePath file_path = path_name.Append(file_name);

    // Explicitly (and redundantly check) for file -- despite the fact that our
    // open won't overwrite -- just to avoid log spew.
    if (!file_util::PathExists(file_path) &&
        file_stream->Open(path_name.Append(file_name),
            base::PLATFORM_FILE_CREATE | base::PLATFORM_FILE_WRITE) == net::OK)
      break;
  }
  if (seq > k_max_seq) {
    delete file_stream;
    return nil;
  }

  // The writer will take care of closing and deletion.
  g_browser_process->file_thread()->message_loop()->PostTask(FROM_HERE,
      new PromiseWriterTask(*dropData_, file_stream));

  // Once we've created the file, we should return the file name.
  return SysUTF8ToNSString(file_name.value());
}

@end  // @implementation WebDragSource


@implementation WebDragSource (Private)

- (void)fillPasteboard {
  DCHECK(pasteboard_.get());

  [pasteboard_ declareTypes:[NSArray array] owner:contentsView_];

  // HTML.
  if (!dropData_->text_html.empty())
    [pasteboard_ addTypes:[NSArray arrayWithObject:NSHTMLPboardType]
                    owner:contentsView_];

  // URL.
  if (dropData_->url.is_valid())
    [pasteboard_ addTypes:[NSArray arrayWithObject:NSURLPboardType]
                    owner:contentsView_];

  // File.
  if (!dropData_->file_contents.empty()) {
    // |dropData_->file_extension| comes with the '.', which we must strip.
    NSString* fileExtension =
        (dropData_->file_extension.length() > 0) ?
        SysUTF16ToNSString(dropData_->file_extension.substr(1)) : @"";

    // File contents (with and without specific type), file (HFS) promise, TIFF.
    // TODO(viettrungluu): others?
    [pasteboard_ addTypes:[NSArray arrayWithObjects:
                                  NSFileContentsPboardType,
                                  NSCreateFileContentsPboardType(fileExtension),
                                  NSFilesPromisePboardType,
                                  NSTIFFPboardType,
                                  nil]
                    owner:contentsView_];

    // For the file promise, we need to specify the extension.
    [pasteboard_ setPropertyList:[NSArray arrayWithObject:fileExtension]
                         forType:NSFilesPromisePboardType];
  }

  // Plain text.
  if (!dropData_->plain_text.empty())
    [pasteboard_ addTypes:[NSArray arrayWithObject:NSStringPboardType]
                    owner:contentsView_];
}

- (NSImage*)dragImage {
  return MakeDragImage(dropData_.get());
}

@end  // @implementation WebDragSource (Private)
