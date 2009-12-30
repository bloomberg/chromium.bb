// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines utility functions for X11 (Linux only). This code has been
// ported from XCB since we can't use XCB on Ubuntu while its 32-bit support
// remains woefully incomplete.

#include "chrome/common/x11_util.h"

#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include <sys/ipc.h>
#include <sys/shm.h>

#include <list>
#include <set>

#include "base/logging.h"
#include "base/gfx/size.h"
#include "base/thread.h"
#include "chrome/common/x11_util_internal.h"

namespace x11_util {

namespace {

// Used to cache the XRenderPictFormat for a visual/display pair.
struct CachedPictFormat {
  bool equals(Display* display, Visual* visual) const {
    return display == this->display && visual == this->visual;
  }

  Display* display;
  Visual* visual;
  XRenderPictFormat* format;
};

typedef std::list<CachedPictFormat> CachedPictFormats;

// Returns the cache of pict formats.
CachedPictFormats* get_cached_pict_formats() {
  static CachedPictFormats* formats = NULL;
  if (!formats)
    formats = new CachedPictFormats();
  return formats;
}

// Maximum number of CachedPictFormats we keep around.
const size_t kMaxCacheSize = 5;

}  // namespace

bool XDisplayExists() {
  return (gdk_display_get_default() != NULL);
}

Display* GetXDisplay() {
  static Display* display = NULL;

  if (!display)
    display = gdk_x11_get_default_xdisplay();

  return display;
}

static bool DoQuerySharedMemorySupport(Display* dpy) {
  int dummy;
  Bool pixmaps_supported;
  // Query the server's support for shared memory
  if (!XShmQueryVersion(dpy, &dummy, &dummy, &pixmaps_supported))
    return false;
  // If the server doesn't support shared memory, give up. (Note that if
  // |shared_pixmaps| is true, it just means that the server /supports/ shared
  // memory, not that it will work on this connection.)
  if (!pixmaps_supported)
    return false;

  // Next we probe to see if shared memory will really work
  int shmkey = shmget(IPC_PRIVATE, 1, 0666);
  if (shmkey == -1)
    return false;
  void* address = shmat(shmkey, NULL, 0);
  // Mark the shared memory region for deletion
  shmctl(shmkey, IPC_RMID, NULL);

  XShmSegmentInfo shminfo;
  memset(&shminfo, 0, sizeof(shminfo));
  shminfo.shmid = shmkey;

  gdk_error_trap_push();
  bool result = XShmAttach(dpy, &shminfo);
  XSync(dpy, False);
  if (gdk_error_trap_pop())
    result = false;
  shmdt(address);
  if (!result)
    return false;

  XShmDetach(dpy, &shminfo);
  return true;
}

bool QuerySharedMemorySupport(Display* dpy) {
  static bool shared_memory_support = false;
  static bool shared_memory_support_cached = false;

  if (shared_memory_support_cached)
    return shared_memory_support;

  shared_memory_support = DoQuerySharedMemorySupport(dpy);
  shared_memory_support_cached = true;

  return shared_memory_support;
}

bool QueryRenderSupport(Display* dpy) {
  static bool render_supported = false;
  static bool render_supported_cached = false;

  if (render_supported_cached)
    return render_supported;

  // We don't care about the version of Xrender since all the features which
  // we use are included in every version.
  int dummy;
  render_supported = XRenderQueryExtension(dpy, &dummy, &dummy);
  render_supported_cached = true;

  return render_supported;
}

int GetDefaultScreen(Display* display) {
  return XDefaultScreen(display);
}

XID GetX11RootWindow() {
  return GDK_WINDOW_XID(gdk_get_default_root_window());
}

XID GetX11WindowFromGtkWidget(GtkWidget* widget) {
  return GDK_WINDOW_XID(widget->window);
}

XID GetX11WindowFromGdkWindow(GdkWindow* window) {
  return GDK_WINDOW_XID(window);
}

void* GetVisualFromGtkWidget(GtkWidget* widget) {
  return GDK_VISUAL_XVISUAL(gtk_widget_get_visual(widget));
}

int BitsPerPixelForPixmapDepth(Display* dpy, int depth) {
  int count;
  XPixmapFormatValues* formats = XListPixmapFormats(dpy, &count);
  if (!formats)
    return -1;

  int bits_per_pixel = -1;
  for (int i = 0; i < count; ++i) {
    if (formats[i].depth == depth) {
      bits_per_pixel = formats[i].bits_per_pixel;
      break;
    }
  }

  XFree(formats);
  return bits_per_pixel;
}

bool IsWindowVisible(XID window) {
  XWindowAttributes win_attributes;
  XGetWindowAttributes(GetXDisplay(), window, &win_attributes);
  return (win_attributes.map_state == IsViewable);
}

bool GetWindowRect(XID window, gfx::Rect* rect) {
  Window root, child;
  int x, y;
  unsigned int width, height;
  unsigned int border_width, depth;

  if (!XGetGeometry(GetXDisplay(), window, &root, &x, &y,
                    &width, &height, &border_width, &depth))
    return false;

  if (!XTranslateCoordinates(GetSecondaryDisplay(), window, root,
                             0, 0, &x, &y, &child))
    return false;

  *rect = gfx::Rect(x, y, width, height);
  return true;
}

bool GetIntProperty(XID window, const std::string& property_name, int* value) {
  Atom property_atom = gdk_x11_get_xatom_by_name_for_display(
      gdk_display_get_default(), property_name.c_str());

  Atom type = None;
  int format = 0;  // size in bits of each item in 'property'
  long unsigned int num_items = 0, remaining_bytes = 0;
  unsigned char* property = NULL;

  int result = XGetWindowProperty(GetXDisplay(),
                                  window,
                                  property_atom,
                                  0,      // offset into property data to read
                                  1,      // max length to get
                                  False,  // deleted
                                  AnyPropertyType,
                                  &type,
                                  &format,
                                  &num_items,
                                  &remaining_bytes,
                                  &property);
  if (result != Success)
    return false;

  if (format != 32 || num_items != 1) {
    XFree(property);
    return false;
  }

  *value = *(reinterpret_cast<int*>(property));
  XFree(property);
  return true;
}

bool GetStringProperty(
    XID window, const std::string& property_name, std::string* value) {
  Atom property_atom = gdk_x11_get_xatom_by_name_for_display(
      gdk_display_get_default(), property_name.c_str());

  Atom type = None;
  int format = 0;  // size in bits of each item in 'property'
  long unsigned int num_items = 0, remaining_bytes = 0;
  unsigned char* property = NULL;

  int result = XGetWindowProperty(GetXDisplay(),
                                  window,
                                  property_atom,
                                  0,      // offset into property data to read
                                  1024,   // max length to get
                                  False,  // deleted
                                  AnyPropertyType,
                                  &type,
                                  &format,
                                  &num_items,
                                  &remaining_bytes,
                                  &property);
  if (result != Success)
    return false;

  if (format != 8) {
    XFree(property);
    return false;
  }

  value->assign(reinterpret_cast<char*>(property), num_items);
  XFree(property);
  return true;
}

XID GetParentWindow(XID window) {
  XID root = None;
  XID parent = None;
  XID* children = NULL;
  unsigned int num_children = 0;
  XQueryTree(GetXDisplay(), window, &root, &parent, &children, &num_children);
  if (children)
    XFree(children);
  return parent;
}

XID GetHighestAncestorWindow(XID window, XID root) {
  while (true) {
    XID parent = x11_util::GetParentWindow(window);
    if (parent == None)
      return None;
    if (parent == root)
      return window;
    window = parent;
  }
}

// Returns true if |window| is a named window.
bool IsWindowNamed(XID window) {
  XTextProperty prop;
  if (!XGetWMName(GetXDisplay(), window, &prop) || !prop.value)
    return false;

  XFree(prop.value);
  return true;
}

bool EnumerateChildren(EnumerateWindowsDelegate* delegate, XID window,
                       const int max_depth, int depth) {
  if (depth > max_depth)
    return false;

  XID root, parent, *children;
  unsigned int num_children;
  int status = XQueryTree(GetXDisplay(), window, &root, &parent, &children,
                          &num_children);
  if (status == 0)
    return false;

  std::set<XID> windows;
  for (unsigned int i = 0; i < num_children; i++)
    windows.insert(children[i]);

  XFree(children);

  // XQueryTree returns the children of |window| in bottom-to-top order, so
  // reverse-iterate the list to check the windows from top-to-bottom.
  std::set<XID>::reverse_iterator iter;
  for (iter = windows.rbegin(); iter != windows.rend(); iter++) {
    if (IsWindowNamed(*iter) && delegate->ShouldStopIterating(*iter))
      return true;
  }

  // If we're at this point, we didn't find the window we're looking for at the
  // current level, so we need to recurse to the next level.  We use a second
  // loop because the recursion and call to XQueryTree are expensive and is only
  // needed for a small number of cases.
  if (++depth <= max_depth) {
    for (iter = windows.rbegin(); iter != windows.rend(); iter++) {
      if (EnumerateChildren(delegate, *iter, max_depth, depth))
        return true;
    }
  }

  return false;
}

bool EnumerateAllWindows(EnumerateWindowsDelegate* delegate, int max_depth) {
  XID root = GetX11RootWindow();
  return EnumerateChildren(delegate, root, max_depth, 0);
}

bool GetXWindowStack(std::vector<XID>* windows) {
  windows->clear();

  static Atom atom = XInternAtom(GetXDisplay(),
                                 "_NET_CLIENT_LIST_STACKING", False);

  Atom type;
  int format;
  unsigned long count;
  unsigned long bytes_after;
  unsigned char *data = NULL;
  if (XGetWindowProperty(GetXDisplay(),
                         GetX11RootWindow(),
                         atom,
                         0,                // offset
                         ~0L,              // length
                         False,            // delete
                         AnyPropertyType,  // requested type
                         &type,
                         &format,
                         &count,
                         &bytes_after,
                         &data) != Success) {
    return false;
  }

  bool result = false;
  if (type == XA_WINDOW && format == 32 && data && count > 0) {
    result = true;
    XID* stack = reinterpret_cast<XID*>(data);
    for (unsigned long i = 0; i < count; i++)
      windows->insert(windows->begin(), stack[i]);
  }

  if (data)
    XFree(data);

  return result;
}

void RestackWindow(XID window, XID sibling, bool above) {
  XWindowChanges changes;
  changes.sibling = sibling;
  changes.stack_mode = above ? Above : Below;
  XConfigureWindow(GetXDisplay(), window, CWSibling | CWStackMode, &changes);
}

XRenderPictFormat* GetRenderVisualFormat(Display* dpy, Visual* visual) {
  DCHECK(QueryRenderSupport(dpy));

  CachedPictFormats* formats = get_cached_pict_formats();

  for (CachedPictFormats::const_iterator i = formats->begin();
       i != formats->end(); ++i) {
    if (i->equals(dpy, visual))
      return i->format;
  }

  // Not cached, look up the value.
  XRenderPictFormat* pictformat = XRenderFindVisualFormat(dpy, visual);
  CHECK(pictformat) << "XRENDER does not support default visual";

  // And store it in the cache.
  CachedPictFormat cached_value;
  cached_value.visual = visual;
  cached_value.display = dpy;
  cached_value.format = pictformat;
  formats->push_front(cached_value);

  if (formats->size() == kMaxCacheSize) {
    formats->pop_back();
    // We should really only have at most 2 display/visual combinations:
    // one for normal browser windows, and possibly another for an argb window
    // created to display a menu.
    //
    // If we get here it's not fatal, we just need to make sure we aren't
    // always blowing away the cache. If we are, then we should figure out why
    // and make it bigger.
    NOTREACHED();
  }

  return pictformat;
}

XRenderPictFormat* GetRenderARGB32Format(Display* dpy) {
  static XRenderPictFormat* pictformat = NULL;
  if (pictformat)
    return pictformat;

  // First look for a 32-bit format which ignores the alpha value
  XRenderPictFormat templ;
  templ.depth = 32;
  templ.type = PictTypeDirect;
  templ.direct.red = 16;
  templ.direct.green = 8;
  templ.direct.blue = 0;
  templ.direct.redMask = 0xff;
  templ.direct.greenMask = 0xff;
  templ.direct.blueMask = 0xff;
  templ.direct.alphaMask = 0;

  static const unsigned long kMask =
    PictFormatType | PictFormatDepth |
    PictFormatRed | PictFormatRedMask |
    PictFormatGreen | PictFormatGreenMask |
    PictFormatBlue | PictFormatBlueMask |
    PictFormatAlphaMask;

  pictformat = XRenderFindFormat(dpy, kMask, &templ, 0 /* first result */);

  if (!pictformat) {
    // Not all X servers support xRGB32 formats. However, the XRENDER spec says
    // that they must support an ARGB32 format, so we can always return that.
    pictformat = XRenderFindStandardFormat(dpy, PictStandardARGB32);
    CHECK(pictformat) << "XRENDER ARGB32 not supported.";
  }

  return pictformat;
}

XSharedMemoryId AttachSharedMemory(Display* display, int shared_memory_key) {
  DCHECK(QuerySharedMemorySupport(display));

  XShmSegmentInfo shminfo;
  memset(&shminfo, 0, sizeof(shminfo));
  shminfo.shmid = shared_memory_key;

  // This function is only called if QuerySharedMemorySupport returned true. In
  // which case we've already succeeded in having the X server attach to one of
  // our shared memory segments.
  if (!XShmAttach(display, &shminfo))
    NOTREACHED();

  return shminfo.shmseg;
}

void DetachSharedMemory(Display* display, XSharedMemoryId shmseg) {
  DCHECK(QuerySharedMemorySupport(display));

  XShmSegmentInfo shminfo;
  memset(&shminfo, 0, sizeof(shminfo));
  shminfo.shmseg = shmseg;

  if (!XShmDetach(display, &shminfo))
    NOTREACHED();
}

XID CreatePictureFromSkiaPixmap(Display* display, XID pixmap) {
  XID picture = XRenderCreatePicture(
      display, pixmap, GetRenderARGB32Format(display), 0, NULL);

  return picture;
}

void FreePicture(Display* display, XID picture) {
  XRenderFreePicture(display, picture);
}

void FreePixmap(Display* display, XID pixmap) {
  XFreePixmap(display, pixmap);
}

// Called on BACKGROUND_X11 thread.
Display* GetSecondaryDisplay() {
  static Display* display = NULL;
  if (!display) {
    display = XOpenDisplay(NULL);
    CHECK(display);
  }

  return display;
}

// Called on BACKGROUND_X11 thread.
bool GetWindowGeometry(int* x, int* y, unsigned* width, unsigned* height,
                       XID window) {
  Window root_window, child_window;
  unsigned border_width, depth;
  int temp;

  if (!XGetGeometry(GetSecondaryDisplay(), window, &root_window, &temp, &temp,
                    width, height, &border_width, &depth))
    return false;
  if (!XTranslateCoordinates(GetSecondaryDisplay(), window, root_window,
                             0, 0 /* input x, y */, x, y /* output x, y */,
                             &child_window))
    return false;

  return true;
}

// Called on BACKGROUND_X11 thread.
bool GetWindowParent(XID* parent_window, bool* parent_is_root, XID window) {
  XID root_window, *children;
  unsigned num_children;

  Status s = XQueryTree(GetSecondaryDisplay(), window, &root_window,
                        parent_window, &children, &num_children);
  if (!s)
    return false;

  if (children)
    XFree(children);

  *parent_is_root = root_window == *parent_window;
  return true;
}

bool GetWindowManagerName(std::string* wm_name) {
  DCHECK(wm_name);
  int wm_window = 0;
  if (!x11_util::GetIntProperty(x11_util::GetX11RootWindow(),
                                "_NET_SUPPORTING_WM_CHECK",
                                &wm_window)) {
    return false;
  }
  if (!x11_util::GetStringProperty(static_cast<XID>(wm_window),
                                   "_NET_WM_NAME",
                                   wm_name)) {
    return false;
  }
  return true;
}

static cairo_status_t SnapshotCallback(
    void *closure, const unsigned char *data, unsigned int length) {
  std::vector<unsigned char>* png_representation =
      static_cast<std::vector<unsigned char>*>(closure);

  size_t old_size = png_representation->size();
  png_representation->resize(old_size + length);
  memcpy(&(*png_representation)[old_size], data, length);
  return CAIRO_STATUS_SUCCESS;
}

void GrabWindowSnapshot(GtkWindow* gtk_window,
                        std::vector<unsigned char>* png_representation) {
  GdkWindow* gdk_window = GTK_WIDGET(gtk_window)->window;
  Display* display = GDK_WINDOW_XDISPLAY(gdk_window);
  XID win = GDK_WINDOW_XID(gdk_window);
  XWindowAttributes attr;
  if (XGetWindowAttributes(display, win, &attr) != 0) {
    LOG(ERROR) << "Couldn't get window attributes";
    return;
  }
  XImage* image = XGetImage(
      display, win, 0, 0, attr.width, attr.height, AllPlanes, ZPixmap);
  if (!image) {
    LOG(ERROR) << "Couldn't get image";
    return;
  }
  if (image->depth != 24) {
    LOG(ERROR)<< "Unsupported image depth " << image->depth;
    return;
  }
  cairo_surface_t* surface =
      cairo_image_surface_create_for_data(
          reinterpret_cast<unsigned char*>(image->data),
          CAIRO_FORMAT_RGB24,
          image->width,
          image->height,
          image->bytes_per_line);

  if (!surface) {
    LOG(ERROR) << "Unable to create Cairo surface from XImage data";
    return;
  }
  cairo_surface_write_to_png_stream(
      surface, SnapshotCallback, png_representation);
  cairo_surface_destroy(surface);
}

}  // namespace x11_util
