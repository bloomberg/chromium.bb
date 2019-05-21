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

### bugfix/caret-position-for-inline-children (Shezan Baig; D48542454) \[[view changes](http://github.com/bloomberg/chromium.bb/compare/upstream%2Fpatched%2Flatest...bugfix%2FcaretPositionForInlineChildren)\]
Open [this link](repros/caretPositionForInlineChildren.html) in Chrome. The
"pill" elements have `-webkit-user-modify: read-only`, however, the caret
still gets rendered within the pills. But when the user types, the text
(correctly) gets inserted outside the pills. Also, the caret is rendered
outside the contenteditable div if there is no text after the pills.

Open [this link](repros/emptycellcaret.html) in Chrome, and put the caret
inside an empty table cell. The caret's `y-position` is outside the
`table-cell`.

### bugfix/empty-new-line-selection (Shezan Baig; upstream: [568663](https://code.google.com/p/chromium/issues/detail?id=568663)) \[[view changes](http://github.com/bloomberg/chromium.bb/compare/upstream%2Fpatched%2Flatest...bugfix%2FemptyNewlineSelection)\]
Open [this link](repros/tableEditing.html) in Chrome, and try to select an
empty line. The line-ending is not highlighted.

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

### feature/dnd-bb-custom-data (Shezan Baig; D40335184) \[[view changes](http://github.com/bloomberg/chromium.bb/compare/upstream%2Fpatched%2Flatest...feature%2FfileEnhancements)\]
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

* * *

###### Microsoft, Windows, Visual Studio and ClearType are registered trademarks of Microsoft Corp.
###### Firefox is a registered trademark of Mozilla Corp.
###### Chrome is a registered trademark of Google Inc.

