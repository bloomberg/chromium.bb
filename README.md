Branches
--------

This section will describe each branch and what changes they introduce. Note
that some of these changes have been contributed upstream. Our intention is to
contribute as many changes upstream as possible so that we do not have to
maintain these branches in our repository.

However, we realize that some of these changes are made in order to adjust the
behavior of our own product, and may not be desirable/applicable to the
general web. For example, the way the `<delete>`/`<backspace>` keys work
inside table cells, the way indenting/outdenting of list items work, etc.
Therefore, we will probably not be able to send _everything_ upstream.

### bugfix/badquerycommand (Shezan Baig; D32128551) \[[view changes](http://github.com/bloomberg/chromium.bb/compare/upstream%2Fpatched%2Flatest...bugfix%2Fbadquerycommand)\]
Open [this link](repros/badquerycommand.html) in Chrome, click the yellow div
and hit `<enter>` a bunch of times. Then `<shift>`+`<up>` a bunch of times.
Each time you press `<up>`, the result of `queryCommandValue('bold')` changes
inconsistently.

### bugfix/caret-position-for-inline-children (Shezan Baig; D48542454) \[[view changes](http://github.com/bloomberg/chromium.bb/compare/upstream%2Fpatched%2Flatest...bugfix%2FcaretPositionForInlineChildren)\]
Open [this link](repros/caretPositionForInlineChildren.html) in Chrome. The
"pill" elements have `-webkit-user-modify: read-only`, however, the caret
still gets rendered within the pills. But when the user types, the text
(correctly) gets inserted outside the pills. Also, the caret is rendered
outside the contenteditable div if there is no text after the pills.

### bugfix/cleartypecanvas (Shezan Baig; D32281407; upstream: [86776](https://bugs.webkit.org/show_bug.cgi?id=86776)) \[[view changes](http://github.com/bloomberg/chromium.bb/compare/upstream%2Fpatched%2Flatest...bugfix%2Fcleartypecanvas)\]
Open [this link](repros/cleartypecanvas) in Chrome, and notice that the
canvas element does not use ClearType fonts even though
`subpixel-antialiased` is used.

Note that when painting to an `ImageBuffer`, Chromium disables ClearType even
if it is the default on the system. However, in our case, we are explicitly
requesting it in the document, so this change makes Chromium honor the
`-webkit-font-smoothing` setting if it is requested explicitly.

Note that there are two distinct changes introduced by this branch:
*   The `-webkit-font-smoothing` setting has no effect in Windows for Skia.
    This is the change that is being tracked upstream by WebKit bug
    [86776](https://bugs.webkit.org/show_bug.cgi?id=86776).
*   Don't disable ClearType on `ImageBuffer` if it was explicitly requested.
    This is not yet being tracked upstream.


### bugfix/cursorContext (Shezan Baig; D35952365) \[[view changes](http://github.com/bloomberg/chromium.bb/compare/upstream%2Fpatched%2Flatest...bugfix%2FcursorContext)\]
It is somewhat hard to reproduce, the window height needs to be just about
right.

Open [this link](repros/cursorContext.html) in Chrome, and hit Enter until
the cursor is at the last possible line where the scrollbar does not appear.
Make sure there is just enough space for about half a line more.

Now keep typing on this line, letting it word-wrap which causes the scrollbar
to appear. Keep doing this for a few lines. You will notice that on some
lines (usually the second or the third, then every alternating line after
that), the scrollbar does not scroll down completely.

In our product, there is an overlay around the RTE of about a couple of
pixels. The purpose of the padding on the contenteditable was to prevent this
overlay from covering the text. However, since the bottom of the text was
right against the border of the `content-editable`, this was causing the
descent of some characters to be obscured.

We "fixed" this in this branch by inflating the reveal rect by about half the
cursor height, showing more "context" when moving up and down in a scrollable
contenteditable. However, a proper fix would be to make the last line scroll
completely, taking the padding into account. We haven't gotten around to this
yet.

### bugfix/deleteBackspace (Shezan Baig; D32947240, D38514960) \[[view changes](http://github.com/bloomberg/chromium.bb/compare/upstream%2Fpatched%2Flatest...bugfix%2FdeleteBackspace)\]
This branch consists of multiple fixes that were merged together:
*   **backspaceIntoTable (D32947240)**  
    Open [this link](repros/backspaceIntoTable.html) in Chrome, and put the
    cursor between 'T' and 'e' in "Testing", and hit backspace twice. The
    rest of the text will go into the last cell.
*   **backspaceOutOfLIInCell (D38514960)**  
    Open [this link](repros/backspaceOutOfLIInCell.html) in Chrome, and try
    to backspace out of the list-item inside the table cell.
*   **nonemptycellbackspace**  
    Open [this link](repros/nonemptycellbackspace.html) in Chrome, and put
    the cursor in the beginning of the second or third cell and start
    backspacing. Note that the issue does not happen when the cell is empty.  
    _Note that this has been fixed upstream since the `28.0.1500.95` snapshot._
*   **nonemptycelldelete**  
    Open [this link](repros/nonemptycelldelete.html) in Chrome, and put the
    cursor at the end of the first cell, after the 'c' and start deleting.
    The table in the next cell will be highlighted on the first press, and
    deleted on the second press. Note that the issue does not happen when the
    cell is empty.  
    _Note that this has been fixed upstream since the `28.0.1500.95` snapshot._

### bugfix/doubleMouseWheel (Imran Haider) \[[view changes](http://github.com/bloomberg/chromium.bb/compare/upstream%2Fpatched%2Flatest...bugfix%2FdoubleMouseWheel)\]
Open [this link](repros/mousewheel.html) in blpwtk2\_shell. Note that this
issue does not reproduce in Chrome because it requires a specific HWND
structure, which is only used in blpwtk2\_shell.

NOTE: Starting in chromium 45, this issue no longer occurs in blpwtk2\_shell,
but does occur in other applications. **TODO: Update the test to reproduce
the issue in blpwtk2\_shell.**

### bugfix/dragSelectionExtent (Shezan Baig) \[[view changes](http://github.com/bloomberg/chromium.bb/compare/upstream%2Fpatched%2Flatest...bugfix%2FdragSelectionExtent)\]
Open the following pages in Chrome and follow the instructions. The selection
extent jumps around in non-intuitive ways. This branch changes how the
selection is extended when dragging with the mouse to be more intuitive.
*   [This link](repros/dragSelectionExtent1.html)
*   [This link](repros/dragSelectionExtent2.html)
*   [This link](repros/dragSelectionExtent3.html)

### bugfix/emptycellcaret (Shezan Baig; upstream: [85385](https://bugs.webkit.org/show_bug.cgi?id=85385)) \[[view changes](http://github.com/bloomberg/chromium.bb/compare/upstream%2Fpatched%2Flatest...bugfix%2Femptycellcaret)\]
Open [this link](repros/emptycellcaret.html) in Chrome, and put the caret
inside an empty table cell. The caret's `y-position` is outside the
`table-cell`.

### bugfix/empty-new-line-selection (Shezan Baig; upstream: [568663](https://code.google.com/p/chromium/issues/detail?id=568663)) \[[view changes](http://github.com/bloomberg/chromium.bb/compare/upstream%2Fpatched%2Flatest...bugfix%2FemptyNewlineSelection)\]
Open [this link](repros/tableEditing.html) in Chrome, and try to select an
empty line. The line-ending is not highlighted.

### bugfix/flexboxStretchAlignment (Shezan Baig) \[[view changes](http://github.com/bloomberg/chromium.bb/compare/upstream%2Fpatched%2Flatest...bugfix%2FflexboxStretchAlignment)\]
Open [this link](repros/flexboxStretchAlignment1.html) in Chrome. The green
box should be 80% the height of the blue box, with a minimum height of 300px.
But in Chrome, the green box height is always 300px.

### bugfix/listify-spans (Shezan Baig; D37507315) \[[view changes](http://github.com/bloomberg/chromium.bb/compare/upstream%2Fpatched%2Flatest...bugfix%2FlistifySpans)\]
Open [this link](repros/listifySpans.html) in Chrome. The second and third
list items will be selected. Click on the `OL` button to convert these items
to an ordered list. The second and third list items get placed into separate
lists, but they should be on the same list. Note that this doesn't happen if
the first and second items are selected instead.

### bugfix/mouse-leave-overlapping-window (Shezan Baig) \[[view changes](http://github.com/bloomberg/chromium.bb/compare/upstream%2Fpatched%2Flatest...bugfix%2FmouseLeaveOverlappingWindow)\]
Open [this link](repros/mouseLeaveOverlappingWindow.html) in Chrome. Then
drag another window so that it partially overlaps the green box. Move the
mouse over to the green box, it will turn red. Now move the mouse to the
overlapping window. It should turn back to green, but because of a bug, it
doesn't.

Open [this link](repros/mouseEnterLeaveEvents.html) in Chrome. Move the mouse
inside and outside the Chrome window. When the mouse is inside, a green box
appears; when the mouse is outside, the box is red. The color changes are
caused by 'mouseenter' and 'mousleave' events firing. Now drag another window
so that it partially overlaps the Chrome window. Move the mouse from the
Chrome window to the overlapping window; the box should turn red, but it
doesn't.

### bugfix/noBRAtStartOfPara (Shezan Baig; D36813987) \[[view changes](http://github.com/bloomberg/chromium.bb/compare/upstream%2Fpatched%2Flatest...bugfix%2FnoBRAtStartOfPara)\]
Open [this link](repros/noBRAtStartOfPara.html) in Chrome. The contents of
the table will be selected on load. Hit the "List" button multiple times.
Each click executes the `insertOrderedList` command.

On every second click, the table will move down.

### bugfix/removeSpellingMarker (Tianyin Zhang; D38695085) \[[view changes](http://github.com/bloomberg/chromium.bb/compare/upstream%2Fpatched%2Flatest...bugfix%2FremoveSpellingMarker)\]
Open [this link](repros/removeSpellingMarker.html) in chrome, place the
cursor inside the contenteditable and make sure some mispelling markers show
up. Then click on the button to switch the contenteditable to false. Note
that the misspelling markers aren't removed.

### bugfix/singleCellDelete (Shezan Baig; D37576223) \[[view changes](http://github.com/bloomberg/chromium.bb/compare/upstream%2Fpatched%2Flatest...bugfix%2FsingleCellDelete)\]
Open [this link](repros/singleCellDelete.html) in Chrome, and delete the text
in the single table cell. The entire table will get deleted, but we only
expect the text inside the cell to be deleted.

### bugfix/stale-tooltip (Shezan Baig; upstream: [84375](https://bugs.webkit.org/show_bug.cgi?id=84375)) \[[view changes](http://github.com/bloomberg/chromium.bb/compare/upstream%2Fpatched%2Flatest...bugfix%2Ftooltip_refresh)\]
Open [this link](repros/tooltip_refresh.html) in Chrome. Each line has a
tooltip set. However, when the first tooltip shows up, and you move the mouse
away from it, it doesn't move the tooltip to the element where the mouse is.

### feature/font-family (Shezan Baig) \[[view changes](http://github.com/bloomberg/chromium.bb/compare/upstream%2Fpatched%2Flatest...feature%2FboldItalicInFontName)\]
This is a feature we needed in our product, so it is unlikely that we will
send this upstream.

In previous versions of WebKit, specifying "font-family: Foo Bold" would
automatically pick the bold version of the font "Foo", even when font-weight
was not set. Unfortunately, we have content out there that took advantage of
this, and now we need to support that behavior forever.

Open [this link](repros/boldItalicInFontName.html) and make sure the
bold/italic fonts are displayed correctly.

### feature/blpwtk2 (Multiple Authors) \[[view changes](http://github.com/bloomberg/chromium.bb/compare/upstream%2Fpatched%2Flatest...feature%2Fblpwtk2)\]
This is our chromium embedding layer.

### feature/direct-comp-disabled-event (Multiple Authors) \[[view changes](http://github.com/bloomberg/chromium.bb/compare/upstream%2Fpatched%2Flatest...feature%2FbbEnhancements)\]
These are extensions to the Web platform that we use in our products.
*   Open [this link](repros/bbDirectCompositingDisabled.html) in Chrome.
    The text should be rendered using cleartype, but it isn't in Chrome (m43).

### feature/css-text (Multiple Authors) \[[view changes](http://github.com/bloomberg/chromium.bb/compare/upstream%2Fpatched%2Flatest...feature%2FbbEnhancements)\]
These are extensions to the Web platform that we use in our products.
*   Open [this link](repros/bbPreWrapText.html) for `-bb-pre-wrap-text` repro.
*   Open [this link](repros/wordBreakKeepAll.html) and resize the window for
    `-bb-keep-all-if-korean` repro.

### feature/color-document-markers (Shezan Baig; D32415776) \[[view changes](http://github.com/bloomberg/chromium.bb/compare/upstream%2Fpatched%2Flatest...feature%2FcolorDocumentMarkers)\]
This is a feature we needed in our product, so it is unlikely that we will
send this upstream.

We needed extra control of the color of the document markers (i.e. the
squiggly lines for spelling errors etc). WebKit hardcodes spelling errors to
red, grammar errors to green. This branch makes it possible for us to specify
the color from the document.

Open [this link](repros/colorDocumentMarkers.html) in Chrome. With this
patch, the mis-spelled words will be underlined in the specified colors.

TODO: make this a CSS property instead of a DOM attribute.

### feature/fileEnhancements (Shezan Baig; D40335184) \[[view changes](http://github.com/bloomberg/chromium.bb/compare/upstream%2Fpatched%2Flatest...feature%2FfileEnhancements)\]
This is a feature we needed in our product, so it is unlikely that we will
send this upstream.

We needed to expose the full path to a dragged file object. Open
[this link](repros/fileEnhancements.html), and drag some files from Windows
Explorer into the page. The DragOver and Drop events should print out the
full path to the file.

### feature/indent-outdent-block (Shezan Baig; D39558769) \[[view changes](http://github.com/bloomberg/chromium.bb/compare/upstream%2Fpatched%2Flatest...feature%2FindentOutdentBlock)\]
This is a complete reimplementation of the "Indent" and "Outdent" algorithm,
called "IndentBlock" and "OutdentBlock". Instead of cloning paragraphs, and
deleting the old paragraphs, this algorithm just moves blocks in and out of
blockquotes. Also, it operates on blocks of text, instead of paragraphs. This
is similar to how MSWord works.

The following links can be used to compare the behavior when this branch is
applied, and when it isn't (e.g. with upstream Chrome):

*   open [this link](repros/indentOutdentBlock_1.html) and just play around
    comparing behavior
*   open [this link](repros/indentOutdentBlock_2.html) and compare
*   open [this link](repros/indentOutdentBlock_3.html) and compare
*   open [this link](repros/indentOutdentBlock_4.html) and compare
*   open [this link](repros/indentOutdentBlock_5.html) and compare
*   Open [this link](repros/doubleindent.html) and position the cursor before
    the logo. Indent the image twice, then outdent the image twice. Without
    this patch, there will be one level of indentation still left
*   Open [this link](repros/indentpre.html). The "Hello" text will be
    selected on load. Each time you indent the text, the "Hello" will get
    duplicated
*   Open [this link](repros/outdentWithBR.html). The indented block will be
    highlighted on load. Click on the `OUTDENT` button. There are now a bunch
    of empty lines added in between. There is an upstream
    [bug 92130](https://bugs.webkit.org/show_bug.cgi?id=92130) for this.

### feature/insert-html-nested (Shezan Baig; D37136356) \[[view changes](http://github.com/bloomberg/chromium.bb/compare/upstream%2Fpatched%2Flatest...feature%2FinsertHTMLNested)\]
This is a feature we needed in our product, so it is unlikely that we will
send this upstream.

We needed to modify the behavior of the `InsertHTML` command. The WebKit
behavior is to prevent the inserted HTML from being nested inside style
spans. This branch adds `InsertHTMLNested` command that allows the HTML to be
nested inside style spans.

Open [this link](repros/insertHTMLNested.html) in Chrome. With this patch,
the "Insert HTML Nested" button works to preserve the font setting. The
default "Insert HTML" button doesn't preserve it.

Open [this link](repros/insertHTMLNested2.html) in Chrome. The word "hello"
will be highlighted. Now press "Insert HTML". The "hello" text will be
replaced with a link. However, we lose one newline between "hello" and
"world". With this patch, the "Insert HTML Nested" button does the same
thing, except that the newlines are preserved.

### feature/list-marker-enhancements (Shezan Baig; D37088493; D32534047; D32422051; upstream: [21709](https://bugs.webkit.org/show_bug.cgi?id=21709)) \[[view changes](http://github.com/bloomberg/chromium.bb/compare/upstream%2Fpatched%2Flatest...feature%2FlistMarkerEnhancements)\]
This branch makes a bunch of enhancements to list markers that were needed in
our product, so it is unlikely that many of them will be sent upstream.

The following changes are included in this branch:
*   **Fixed alignment for list markers**  
    Open [this link](repros/listMarkerAlignment.html) in Chrome. The list
    markers are all left-aligned, even though the second and third `li`
    elements are not. This particular change may be able to upstream because
    there is already a WebKit bug report for it.
*   **The font used for the list marker will be the same as the first
    character in the list item.**  
    Open [this link](repros/listMarkerFont.html) in Chrome. The list item
    will be selected. When you click `BIGGER` or `SMALLER` to change the font
    size, the list marker's font does not change with it. In our product, we
    wanted the list marker's font to match the first character in the list
    item.
*   **The list markers are not highlighted when the list item is selected**  
    This matches the behavior in Firefox and Microsoft Word software.  
    Open [this link](repros/listMarkerSelection.html) in Chrome. Notice that
    the list markers are highlighted. Additionally, notice that the list
    marker highlight does not honor the `::selection` background color
    specified in the stylesheet. In our product, we didn't want the list
    markers to be highlighted when selected.
*   **Change layout of list items and list markers**  
    The way list elements and list items are laid out has been changed so
    that, instead of `OL`/`UL` elements having 40px padding, the `LI`
    elements will receive an additional (intrinsic) margin equivalent to the
    width of the list marker. This matches more closely what other software
    like Microsoft Word does, and also prevents issues like
    [this link](repros/listMarkerEnhancements1.html) where the list marker
    gets pushed outside the page.

### feature/remove-ime-underline-color (Calum Robinson; D47474798) \[[view changes](http://github.com/bloomberg/chromium.bb/compare/upstream%2Fpatched%2Flatest...feature%2FremoveIMEUnderlineColor)\]
This is a feature we needed in our product, so it is unlikely that we will
send this upstream.

By default, Chrome shows a black underline while composing in IME mode. This
is a problem if the background is also black. This branch removes the color
property from the `CompositionUnderline` object, and makes `InlineTextBox`
use the text foreground color instead.

Open [this link](repros/removeIMEUnderlineColor.html) in Chrome and type in
Japanese. Without this patch, the underline color will be black, but it will
be yellow once the patch is applied.

### feature/setDataNoRelayout (Shezan Baig; D42070031) \[[view changes](http://github.com/bloomberg/chromium.bb/compare/upstream%2Fpatched%2Flatest...feature%2FsetDataNoRelayout)\]
This is a feature we needed in our product, so it is unlikely that we will
send this upstream.

There are situations where we want to update a span, and because we know the
span is of a fixed size, we don't want the new text to trigger a relayout.
For this, we introduced a new method on text nodes "bbSetDataNoRelayout".

Open [this link](repros/setDataNoRelayout.html) in Chrome (may take a while to
finish loading). With this patch applied, there will be a noticable difference
in the time taken to execute the button click, depending on whether the
"norelayout" checkbox is enabled or disabled.

Retired branches
================

### bugfix/iframeZoom (Shezan Baig; D34484409) \[[view changes](http://github.com/bloomberg/chromium.bb/compare/upstream%2Fpatched%2Flatest...bugfix%2FiframeZoom)\]
Open [this link](repros/iframeZoom.html) in Chrome, and zoom the page in and
out. The content outside the `iframe` zooms, but the content inside the
`iframe` does not zoom.

### bugfix/selrectantialias (Shezan Baig; D32113976; upstream: [87157](https://bugs.webkit.org/show_bug.cgi?id=87157)) \[[view changes](http://github.com/bloomberg/chromium.bb/compare/upstream%2Fpatched%2Flatest...bugfix%2Fselrectantialias)\]
Open [this link](repros/selrectantialias.html) in Chrome. You will see "gaps"
between the lines and the spans on the first line.

The issue is that the selection highlight is drawn for each text run with anti-aliasing on. This causes "gaps" to appear around the edges where the filled rects got blended. The solution was to simply turn off anti-aliasing when filling selection rects.

### feature/caretColor (Claudio Saavedra <csaavedra@igalia.com>; D38294226) \[[view changes](http://github.com/bloomberg/chromium.bb/compare/upstream%2Fpatched%2Flatest...feature%2FcaretColor)\]
This is a feature we needed in our product, so it is unlikely that we will
send this upstream.

We needed to be able to modify the caret color independently of the font
color.

Open [this link](repros/caretColor.html) in Chrome. With this patch, the
caret color in the entry fields will be as specified, otherwise they will be
just black.

### feature/preserveSelDirection (Shezan Baig; D36076457) \[[view changes](http://github.com/bloomberg/chromium.bb/compare/upstream%2Fpatched%2Flatest...feature%2FpreserveSelDirection)\]
This is a feature we needed in our product, so it is unlikely that we will
send this upstream.

This branch makes it so that when text is selected inside a contenteditable,
and then a style is applied on that text, the original selection direction
would be preserved.

Open [this link](repros/preserveSelDirection.html) in Chrome. With this
patch applied, the anchor offset would be 5 positions higher than the focus
offset. Without the patch, the anchor offset and focus offset will be
switched.

### feature/table-cell-selection (Shezan Baig) \[[view changes](http://github.com/bloomberg/chromium.bb/compare/upstream%2Fpatched%2Flatest...feature%2FtableCellSelection)\]
This is a feature we needed in our product, so it is unlikely that we will
send this upstream.

Open [this link](repros/tableEditing.html) in Chrome and select the text in
the table cells. With this patch applied, a table cell's background will be
filled with the selection color when all the text in the cell is selected.


* * *

###### Microsoft, Windows, Visual Studio and ClearType are registered trademarks of Microsoft Corp.
###### Firefox is a registered trademark of Mozilla Corp.
###### Chrome is a registered trademark of Google Inc.

