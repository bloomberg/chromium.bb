# Introduction
The common approach used in printing on Linux is to use Gtk+ and Cairo libraries. The [Gtk+ documentation](http://library.gnome.org/devel/gtk/stable/Printing.html) describes both high-level and low-level APIs for us to do printing. In an application program, the easiest way to do printing is to use [GtkPrintOperation](http://library.gnome.org/devel/gtk/stable/gtk-High-level-Printing-API.html) to get the Cairo context in `draw-page`'s callback, and render **each** page's contents on this specific context, the rest is easy and trivial.

However, in Chromium's multi-process architecture, we hope that all rendering should be done in the renderer process, and I/O should be done in the browser process. The problem is that we are unable to pass the Cairo context we obtained in the browser process to the renderer via IPC and/or shared memory, and later get it back after the rendering is done. Hence, we have to find something which we can _pickle_ and pass between processes.

# Possible Solutions
  1. **Bitmap**: This seems easy because we have already passed bitmaps for displaying web pages. It is also pretty easy to _dump_ this bitmap on the printing context. However, the bitmap takes lots of memory space even when you print a blank page. You might wonder why it can be a critical problem, since we've used bitmaps for displaying. The critical part is that the screen DPI is around 72~110, at least lower than 150. However, the DPI for printing is usually above 150, and maybe 1200 or 2400 for high-end printers. The 72-DPI bitmap actually looks terrible on the paper and reminds us the old time when we used dot-matrix printers. A 72-DPI bitmap will take ~7MB memory (assume we are using US letter paper), hence, it will take ~500MB memory per page when we would like to print with a 600-DPI laser printer. By the way, even we would like to do so, we still have the problem that WebKit seems not to take the DPI factor into account when rendering the web page.
  1. **Rendering records** (scripts): We might be able to record every operation which is going to be performed on canvas, and later pass all these records to the browser side to playback. We can define our own format (or script), or we can use CairoScript. Unfortunately, CairoScript is still in the snapshot (it is under development and we can hardly find its detailed and useful documentation), not in the stable release. Even it is in a stable release, we still cannot assume that the user will install the latest version of Cairo library. If we would like to create our own format/script and use it, we have to replay these records in the browser process, which seems to be another kind of rendering action (it is actually). By the way, one thing we need to take care of would be, for example, when we have to composite multiple semi-transparent bitmaps, we also have to embed these bitmaps along with other skia objects into our records. This implies that we have to be able to _pickle_ `SkBitmap`, `SkPaint`, and other related skia objects. This sucks.
  1. **Metafile approach 1**: We can use Cairo to create the PDF (or PS) file in the renderer (one file per page) and pass it to the browser process. We can then use [libpoppler](http://poppler.freedesktop.org/) to render the page content on the printing context (it is pretty easy to use and we just need to add few lines to use it). This sounds better, but this also means that we have to bring [libpoppler](http://poppler.freedesktop.org/) into our dependency. Moreover, we still have to do _rendering_ in the browser process, which we should really avoid if possible.
  1. **Metafile approach 2**: Again, we use Cairo to create the PDF (or PS) file in the renderer. However, this time we have to generate the PDF/PS file for all pages. Unlike other approaches we mentioned earlier, all rendering tasks are done in the renderer, including transformation for setting up the page, and rendering of the header and footer. Since we do not want to do rendering in the browser process, this means that we cannot use the [GtkPrintOperation](http://library.gnome.org/devel/gtk/stable/gtk-High-level-Printing-API.html) approach, which does require rendering. Instead, we can use [GtkPrintUnixDialog](http://library.gnome.org/devel/gtk/stable/GtkPrintUnixDialog.html) to get printing parameters and generate the [GtkPrintJob](http://library.gnome.org/devel/gtk/stable/GtkPrintJob.html) accordingly. Then, we use `gtk_print_job_set_source_file()` and `gtk_print_job_send ()` to send our PDF/PS file directly to the printing system (CUPS). One bad part is that `gtk_print_job_set_source_file()` only takes a file on the disk, so we have to create a temporary file on the disk before we use it. This file might be pretty large if the user is printing a long web page.

# Our Choice
We currently are using Metafile approach 2, since we really like to avoid any rendering in the browser process if possible. By the way, we are using a two-pass rendering in `PdfPsmetafile` right now. Because in the first pass, we need to get the shrink factor from WebKit so that we can scale and center the page in the second pass. If we later we can have the shrink factor from Preview, we might be able to use single-pass rendering. However, using two-pass rendering might still have some advantages. For example, we can actually do Preview and the first pass at the same time if we use the bitmap object in the `VectorPlatformDevice` as Preview. (Not very sure if it works or not now, since Previewing is also a complicated issue.) Once we have the page settings (margins, scaling, etc), we can easily apply the first-pass results to generate our final output by copying Cairo surfaces.

(Please NOTE the approach used here might be changed in the future.)

# Current Status
We now can generate a PDF file for the web page and save it under user's default download directory. The function is still very basic. Please see ideal Goal, Known Issues, and Bugs below.


---


# Ideal Goal
Design a better printing flow for Linux:
> Ideally, when we print the web page, we should get the _snapshot_ of that page. In the current architecture, we cannot halt JavaScript from running when we print the web page. This is not good since the script might close the page we are printing. Things could be worse if plug-ins are involved. When we print, the renderer sends a sync message to the browser, so the renderer must wait for the browser. We potentially may have deadlocks when the plug-in talks to the renderer. Please see [here](http://dev.chromium.org/developers/design-documents/printing) for further detail. This might be avoided if we could copy the entire DOM tree before we print. It seems that WebKit does not support this directly right now. Before we can entirely solve this issue, we might need to reduce the time and chance we block the renderer. For example, unlike the windows version, we always have at least one printer (print to file). Hence, we can put "Page Setup" in the browser menu, so that we don't need to ask the user each time before we print (You can see this in Firefox and many other Linux applications).
> Another issue is that we might need different mechanisms for different platforms. Obviously, the ways how we do printing on Windows and on Linux are quite different. The printing flow on Linux might be something like this one:
    * Print on a low-resolution bitmap canvas to generate Previews. (Believe it or not! It's actually much more difficult/tedious than it sounds.)
    * We use the preview to do Page Setup: Paper size, margins, page range, and maybe also the header and the footer.
    * Generate the PDF file.
    * Save the resulting PDF as a temporary file.
    * Use GTK+ APIs in the browser to ask the user which printer to use, then directly send the temporary file to CUPS.
> These steps look simple, but we actually need to consider and design more details before we can make it happen. (For example, do we have to support all options shown in the GTK+ printing dialog?)

# Known Issues
  1. For some reason, if we send the resulting PDF files directly to CUPS, we often get nothing without any error message. The CUPS I was using is version 1.3.11. This might be a bug in Cairo 1.6.0, and/or a bug in the PDF filter (pdftopdf? pdftops?) in CUPS. Actually, if we use Firefox and print to file, we will sometimes have this problem, too. Nevertheless, the resulting PDF can be viewed in all PDF viewers without any error. However, you won't see the embedded font information in some PDF viewers, such as evince. If the printer supports PDF natively and has the HTTP interface, we can get the printout by sending the PDF file via printer's HTTP interface. [Issue# 21599](http://code.google.com/p/chromium/issues/detail?id=21599)
  1. WebKit does not pass original text information to skia. Hence, we only have glyphs in the resulting PDF. This implies that we cannot do text selection in the resulting PDF. [Issue# 21602](http://code.google.com/p/chromium/issues/detail?id=21602)
  1. The vector canvas used for printing in skia still has a bitmap within it [Issue# 21604](http://code.google.com/p/chromium/issues/detail?id=21604). This wastes lots of memory and does nothing. Maybe we can use this bitmap to do previewing, or use it as a thumbnail. of course, another possibility might be implementing PDF generating capabilities in skia.
  1. To let Cairo use correct font information, we use FreeType to load the font again in `PdfPsMetafile`. This again wastes lots of memory when printing. It would be nice if we can find a way to share font information with/from skia. [Issue# 21608](http://code.google.com/p/chromium/issues/detail?id=21608)
  1. Since we ask the browser open a temporary file for us. This might potentially be a security hole for DoS attack. We should find a way to limit the size of temporary files and the frequency of creation. (Do we have this now?) [Issue# 21610](http://code.google.com/p/chromium/issues/detail?id=21610)
  1. In Cairo 1.6.0, the library opens a temporary file when creating a PostScript surface. Hence, our only choice is the PDF surface, which does not require any temporary file in the renderer.
  1. In Cairo 1.6.0, we cannot output multiple glyphs at the same time. (we have to do it one by one). Newer version does support multiple glyphs output. We can use it in the future.
  1. I did not have enough time to write good unit tests for classes related to printing on Linux. We definitely need those unit tests in the future. [Issue# 21611](http://code.google.com/p/chromium/issues/detail?id=21611)
  1. I did not have enough time to compare our results with other competitors. Anyway, in the future, we should always compare quality, correctness, size, and maybe also resources and time in printing.
  1. We do not supports all APIs in `SkCanvas` now ([Issue# 21612](http://code.google.com/p/chromium/issues/detail?id=21612)). By the way, when we need to do alpha composition in canvas, the result generated by Cairo is not perfect(buggy). For example, the resulting color might be wrong, and sometimes we will have round-off error in images' layout. You can try to print `third_party/WebKit/LayoutTests/svg/W3C-SVG-1.1/filters-blend-01-b.svg` and compare the result with your screen. If you print it out with a printer using CYMK, you might have incorrect colors.
  1. We should find a way to do layout tests for printing. [Issue# 21613](http://code.google.com/p/chromium/issues/detail?id=21613) For example, it looks not quite right when you print this [page](http://code.google.com/p/chromium/issues/detail?id=8551&colspec=ID%20Stars%20Pri%20Area%20Type%20Status%20Summary%20Modified%20Owner%20Mstone).

# Bugs
  1. There are still many bugs in vector canvas. I did not implement path effect, so it prints dashed lines as solid lines. [Issue# 21614](http://code.google.com/p/chromium/issues/detail?id=21614)
  1. When you print the "new-tab-page", the rounded boxes look strange. [Issue# 21616](http://code.google.com/p/chromium/issues/detail?id=21616)
  1. The button is shown as a black rectangle. We should print it with a bitmap. Of course, we have to get the correct button according to the user's theme. [Issue# 21617](http://code.google.com/p/chromium/issues/detail?id=21617)
  1. The font cache in `PdfPsMetafile` might not be thread-safe. [Issue# 21618](http://code.google.com/p/chromium/issues/detail?id=21618)
  1. The file descriptor map used in the browser might not be thread safe. (However, it is just used at this moment as a quick ugly hack. We should be able to get rid of it when we implement all other printing classes.)
  1. Since we save the resulting PDF file in the renderer, this might not be a good thing and might freeze the renderer for a while. We should find a way to get around it. By the way, maybe we should also show the printing progress? [Issue#21619](http://code.google.com/p/chromium/issues/detail?id=21619)


---


# Reference
[Issue# 9847](http://code.google.com/p/chromium/issues/detail?id=9847)
> It is blocked on [Issue# 19223](http://code.google.com/p/chromium/issues/detail?id=19223)
| Revision# | Code review# |
|:----------|:-------------|
| `r22522`  | `160347`     |
| `r23032`  | `164025`     |
| `r24243`  | `174042`     |
| `r24376`  | `173368`     |
| `r24474`  | `174468`     |
| `r24533`  | `173516`     |
| `r25615`  | `172115`     |
| `r25974`  | `196071`     |
| `r26308`  | `203062`     |
| `r26400`  | `200138`     |