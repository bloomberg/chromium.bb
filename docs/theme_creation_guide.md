
# Theme Creation Guide

The Google Chrome Extensions Help Docs provided some info on how to create theme as an extension, but for a pure designer, the details of the `*`.cc file can be overwhelming and confusing. Also, having a clear documentation, enables a new designer to start designing on the go! (Also makes life easier... he he he).

Experimenting with the creation of theme and the possible UI elements that could be themed helped create this help document(working progress).It would be helpful if **people contribute** to this document in any possible way, that would make it a good Theme Creation Guide!

---

**So how do you create a theme for Google chrome?**

**Things you'll need to create a theme**
  1. A basic text editor (preferably one that shows line numbers-because on packaging a theme, Chrome might point out the error in the control file - manifest.json, if any. It is recommended using Notepad++ which is a free and very useful editor!).
  1. An image editor - preferably an advance editor that can allow you to create good content (Using simple editors can do the job of creating themes, but very sloppy ones! It is recommended to use Photoshop, alternatively you may use the free editors like Gimp and Paint.net, [click here](http://sixrevisions.com/graphics-design/10-excellent-open-source-and-free-alternatives-to-photoshop/)).
  1. If you are using Photoshop, you can download [this Chrome window design](http://www.chromium.org/user-experience/visual-design/chrome_0.2_psd.zip) which is broken down in layers, and makes it easy to visualize what the theme should look like.
  1. Some creative ideas about what the theme is going to look like - the colors, patterns and design.
  1. Package your theme and publish it in one of the following ways -
    1. [Upload](https://chrome.google.com/webstore/developer/dashboard) the theme to the [Chrome Web Store](https://chrome.google.com/webstore/)
    1. Use Chrome to package it by yourself. More information can be found [here](http://code.google.com/chrome/extensions/hosting.html)
    1. Package it by yourself. More information can be found [here](http://code.google.com/chrome/extensions/packaging.html).

_Now that you have the needed tools, let's get started._

First create a folder with the name of the theme, inside it you need to create a folder (usually named _images_, but it's your choice).

_Then you need to create two things:_
The first part is to create the images (PNG images) needed for the theme and put them in the _images_ folder (in the next section you'll see a list of images that can be created for a theme), then create a file named "manifest.json", it needs to be inside the theme folder (here is an example file [manifest.json](http://src.chromium.org/viewvc/chrome/trunk/src/chrome/test/data/extensions/theme/manifest.json?revision=72690&view=markup), open it with basic text editor to see the contents and remember that all notation in this file is in **lower case**)

Then we package the theme and test it.

There are a number of things that can be themed in Chrome.

(See [Description of Elements](#Description_of_Elements.md) section for detailed explanation.)

### Image Elements
_Image elements are defined under the "images" section in the manifest.json file._

|Number|Description|manifest.json Notation |Recommended Size (W x H)|
|:-----|:----------|:----------------------|:-----------------------|
|1     |The frame of the chrome browser/the area that is behind the tabs.|["theme\_frame"](#theme_frame.md)|∞ x 80                |
|1. 1  |The same area as above, only that this represents the inactive state.|["theme\_frame\_inactive"](#theme_frame_inactive.md)|                        |
|1. 2  |The same area under the incognito mode, when the window is active|["theme\_frame\_incognito"](#theme_frame_incognito.md)|                        |
|1. 3  |The same area but in the incognito mode, when the window is inactive.|["theme\_frame\_incognito\_inactive"](#theme_frame_incognito_inactive.md)|                        |
|2     |This represents both the current tab and the toolbar together|["theme\_toolbar"](#theme_toolbar.md)|∞ x 120               |
|3     |This is the area that covers the tabs that are not active|["theme\_tab\_background"](#theme_tab_background.md)|∞ x 65                |
|3. 1  |The same thing as above, but used for the incognito mode|["theme\_tab\_background\_incognito"](#theme_tab_background_incognito.md)|                        |
|4     |(Not yet confirmed) The tab background for something!|["theme\_tab\_background\_v"](#theme_tab_background_v.md)|                        |
|5     |This is the theme's inner background-the large white space is covered by this|["theme\_ntp\_background"](#theme_ntp_background.md)|Recommended Minimum Size for images  800 x 600 |
|6     |This is the image that appears at the top left of the frame|["theme\_frame\_overlay"](#theme_frame_overlay.md)|1100 x 40               |
|6. 1  |Same as above but displayed when window is inactive|["theme\_frame\_overlay\_inactive"](#theme_frame_overlay_inactive.md)|                        |
|7     |This is the area that covers the toolbar buttons|["theme\_button\_background"](#theme_button_background.md)|30 x 30                 |
|8     |This is the image that will be displayed in the 'theme created by' section|["theme\_ntp\_attribution"](#theme_ntp_attribution.md)|                        |
|9     |The background for the window control buttons (close, maximize, etc.,)|["theme\_window\_control\_background"](#theme_window_control_background.md)|                        |


### Color Elements
_Color elements are defined under the "colors" section in the manifest.json file._

Colors are entered as RGB values, some elements can contain opacity value also.
e.g. `"ntp_section" : [15, 15, 15, 0.6]`


|Number | Description |manifest.json Notation|
|:------|:------------|:---------------------|
|10     |The color of the frame, that covers the smaller outer frame|["frame"](#Frame.md)  |
|10. 1  |The color of the same element, but in inactive mode|["frame\_inactive"](#Frame_inactive.md)|
|10. 2  |The color of the same element, but in incognito mode|["frame\_incognito"](#Frame_incognito.md)|
|10. 3  |The color of the same element, but in incognito, inactive mode|["frame\_incognito\_inactive"](#Frame_incognito_inactive.md)|
|10. 4  |The color of the toolbar background (visible by pressing Ctrl+B)|["toolbar"](#toolbar.md)|
|11     |The color of text, in the title of current tab|["tab\_text"](#tab_text.md)|
|12     |The color of text, in the title of all inactive tabs|["tab\_background\_text"](#tab_background_text.md)|
|13     |The color of the bookmark element's text|["bookmark\_text"](#bookmark_text.md)|
|14     |The theme's inner background color|["ntp\_background"](#ntp_background.md)|
|14. 1  |The color of all the text that comes in the inner background area|["ntp\_text"](#ntp_text.md)|
|14. 2  |The color of the links that appear in the background area|["ntp\_link"](#ntp_link.md)|
|14. 3  |The color of the underline of all links in the background area|["ntp\_link\_underline"](#ntp_link_underline.md)|
|14. 4  |The color of the section frames when mouse over|["ntp\_header"](#ntp_header.md)|
|14. 5  |The color of Recently closed tabs area's bg and frame of quick links|["ntp\_section"](#ntp_section.md)|
|14. 6  |The color of text in the section|["ntp\_section\_text"](#ntp_section_text.md)|
|14. 7  |The color of the links that appear in the section area|["ntp\_section\_link"](#ntp_section_link.md)|
|14. 8  |The color of underline of links in the section area|["ntp\_section\_link\_underline"](#ntp_section_link_underline.md)|
|15     |Unconfirmed yet-The color of the window control buttons (close, maximize, etc.)|["control\_background"](#control_background.md)|
|16     |The background color of all the toolbar buttons|["button\_background"](#button_background.md)|

### Tint Elements
Tint elements change the hue, saturation and lightness of images.

_Tint elements come under the "tints" section in the manifest.json file._

|Number|Description|manifest.json Notation|
|:-----|:----------|:---------------------|
|17    |The color tint that can be applied to various buttons in chrome|["buttons"](#buttons.md)|
|18    |The color tint that can be applied to the frame of chrome|["frame"](#frame.md)  |
|18. 1 |The color tint that is applied when the chrome window is inactive|["frame\_inactive"](#frame_inactive.md)|
|18. 2 |The color tint to the frame-in incognito mode|["frame\_incognito"](#frame_incognito.md)|
|18. 3 |Same as above, but when the window is inactive (and in incognito mode)|["frame\_incognito\_inactive"](#frame_incognito_inactive.md)|
|19    |The color tint of the inactive tabs in incognito mode|["background\_tab"](#background_tab.md)|

### UI Property Elements
_Property elements come under the "properties" section in the manifest.json file._

|Number|Description|manifest.json Notation|
|:-----|:----------|:---------------------|
|20    |The property that tells the alignment of the inner backrground image|["ntp\_background\_alignment"](#ntp_background_alignment.md)|
|21    |This property specifies if the above background should be repeated|["ntp\_background\_repeat"](#ntp_background_repeat.md)|
|22    |This lets you select the type of google chrome header you want|["ntp\_logo\_alternate"](#ntp_logo_alternate.md)|

_Phew! lots of things to theme! Actually not!_

These are the elements that google chrome allows a user to theme, but it's the user's whish to decide what elements are going to be edited. The things that you don't need changed can be left alone (in case of which those elements will have their default value/image).Remember that each element goes into it's own section in the manifest file - color elements should be listed under "colors", image elements under "images" and so on.

_Let's go through the elements one by one._

## Description of Elements

### Basic Theme Elements
_These elements are the starting point, by using only them, you can quickly create a basic theme._

  * #### theme\_frame

This is an image, this image represents the area behind the tabs. There is no strict dimensions for this image, the rest of the area in the frame that is not covered by this image is covered by the color element [frame](#Frame.md). It would be helpful to know that this image by default repeats along the x-axis. Hence if you create a small square image, it will be automatically repeated along x-axis-which means you can create patterns if you use short sized images.

Remember this image doesn't repeat along-y, hence make sure it is long enough to cover the toolbar area-anything over 80px height is good, usually with grading alpha transparency at the bottom so that the image blends with the "frame" color.(you can create a large sized frame image, that extends and coveres the frame borders too)

Else you might see a small seperation to the extreme top left of the frame, when the window is in restored mode due to the wrong size of the image.

Alternatively one can decide to create an image with loooong width-long enough that the image repetition is not seen-this method allowes you to create one continuous design for the frame-but this method might slow down the loading time of the theme since large resolution screens require image of larger width (or else you'll see the repetition of the image).Note that if you don't include this image, the default frame of chrome-the blue one is displayed, the color element ["frame"](#Frame.md) doesn't override this.

  * #### theme\_toolbar

This is an image that covers the area of the current tab and the toolbar below it:

Make sure this image is over 119px in height because the find bar( which appears when you press Ctrl+F )shares the tool bar image, the width is up to you. Similar to the theme\_frame, this image also tiles along the x-axis so you have the option to create pattern or create a looong width image for the toolbar. Remember that the toolbar contains some buttons and when the bookmarks are visible (CMD+B or Ctrl+B), they too occupy space in the toolbar:

So don't make the design too much crowded, or else the toolbar will not be visually appealing. Usually for the toolbar, a square, tiling image is preferred, which might be a gradient or just plain color.

  * #### theme\_tab\_background

This is an image, this represents the tabs - all the inactive tabs.

usually a less saturated image of theme\_toolbar is used for this. You may also design something else, but make sure that the design enables the user to distinguish the inactive tabs from the active one!
This image also tiles default in x-axis and the height of this can be around 65px , the width is up to you.

  * #### theme\_ntp\_background
This is the image that is displayed at the large white space in the browser, in the new tab page, it can contain a background image that contains alpha transparency( the default page that contains various quick access elements-see the help image).Note that the notation ntp represents new tab page, hence all elements which contain ntp in the notation will correspond to some element inside the new tab page.

There are two ways you can create the inner background for the browser-use a large image without repetition/tiling or use a small image that repeats in x-axis and/or y-axis.(see [ntp\_background\_repeat](#ntp_background_repeat.md))

There is also option for you to select the alignment of this image, by default the image is center aligned, but you may choose to align it the way you want.(see [ntp\_background\_alignment](#ntp_background_alignment.md))

### Advanced Theme Elements
_Use these to create a more advanced theme._
  * #### theme\_frame\_inactive

This is an image, representing the area behind the tabs, when the chrome window is out of focus/inactive.

All that is applicable to [theme\_frame](#theme_frame.md), applies to this. Usually to avoid making the theme heavy, you can go for [frame\_inactive](#frame_inactive.md) tint, to show that the window is inactive-it's efficient than creating a whole new image. But it's up to the designer to decide, if it's going to be an image seperately for the inactive state or there is going to be a colo tint when the window is inactive.

  * #### theme\_frame\_incognito

This is similar to the [theme\_frame](#theme_frame.md), but this image represents the frame of a window in incognito mode. You may choose to redesign the image specially for the incognito mode or ignore this, so that whatever you made for [theme\_frame](#theme_frame.md) will be tinted (see [frame\_incognito](#frame_incognito.md)) and used in incognito mode (it's by default that it gets a dark tint in incognito mode).

  * #### theme\_frame\_incognito\_inactive

This is also an image, similar to theme\_frame\_inactive, but this image is for the inactive frame of a window in incognito mode.(see [frame\_incognito\_inactive](#frame_incognito_inactive.md))

  * #### theme\_tab\_background\_incognito

This is an image, that represents the inactive tabs, in the incognito mode. Alternatively one can use the tinting [background\_tab](#background_tab.md), to effect inactive tabs in incognito mode, but there is a slight problem that some may want to avoid - even if you tint the inactive tabs of the incognito window, the inactive tabs are made transparent (by default). Hence they'll show the area behind them. i.e. the frame. If you want to avoid this, you can include this image.

  * #### theme\_tab\_background\_v

Until now, the role of this image is a mystery, that someone needs to unlock!

  * #### theme\_frame\_overlay

This is the image that will be displayed at the top left corner of the frame,  over the [theme\_frame](#theme_frame.md) image.Also this image doesn't repeat by default.Hence this image may be used in case you don't want the frame area design to repeat.Similar to the theme\_frame ,anything over 80px height is good, usually with grading alpha transparency at the bottom so that the image blends with the "frame" color.

  * #### theme\_frame\_overlay\_inactive

This is similar to [theme\_frame\_overlay](#theme_frame_overlay.md), but will be displayed when the browser window is inactive.If you do not include this image, theme\_frame\_overlay image will be darkly tinted and used by default-to denote the inactive frame.

  * #### theme\_button\_background

This is the image that specifies the background for various buttons(stop,refresh,back,forward,etc.,) in the toolbar.This image is optional, if you do not include this image, the color element [button\_background](#button_background.md) overrides the button's background color.

Whatever image you give for this, the browser leaves off two pixels at top and left of the image and mapps a square 25px image to the buttons as background.And the icon/symbol of the button(stop,refresh,back,forward,etc.,) is displayed at the center.

  * #### theme\_ntp\_attribution
This is the image that is displayed at the bottom right corner of the new tab page.Chrome automatically puts a heading "Theme created by" and below that displays whatever image you give as theme\_ntp\_attribution.

A good practice is to create a small png file enough for an aurthor name(and contact if needed) with alpha transparency background.Making large and more color intense image will attract view, but will make the theme a bit heavier(the file size of the theme may increase with bigger png file) but it's your choice anyway.

  * #### theme\_window\_control\_background

This is the image that specifies the background for the window control buttons(minimize,maximize,close and new tab).This image is also not necessary until you desperatly need to change the control button background.If the image is included, the browser leaves off 1px at the top and left of image and maps a 16px height button from it, the width varies according to buttons though.

If this image is not included, the control buttons assume the background color specified in the color element button\_background.

<a href='Hidden comment:  NOTE: The following three section headings Frame,Frame_inactive,Frame_incognito,Frame_incognito_inactive  - all these contain capitalised F intentionally so that internal page navigation is possible'></a>

  * #### Frame

This is a color element, that specifies the color of the frame area of the browser(the area behind the tabs + the border).It occupies the area that is not covered by the [theme\_frame](#theme_frame.md) image.
The format to specify this element in the manifest.json file is : `"frame" : [R,G,B]`

  * #### Frame\_inactive

This is a color element, that specifies the color of the frame area of the browser but when the window is inactive/out of focus (the area behind the tabs + the border).It occupies the area that is not covered by the [theme\_frame](#theme_frame.md) image.
The format to specify this element in the manifest.json file is : `"frame_inactive" : [R,G,B]`

  * #### Frame\_incognito

This is a color element similar to ["frame"](#Frame.md) ,but under the incognito mode.

  * #### Frame\_incognito\_inactive

This is a color element similar to ["frame\_inactive"](#Frame_inactive.md) ,but under the incognito mode.

  * #### toolbar

This is a color element that specifies the background color of the bookmarks bar, that is visible only in the new tab page,when you press the shortcut keys Ctrl+B or CMD+B.And it contains a 1px border whose color is defined by [ntp\_header](#ntp_header.md).Also this element can contain an opacity value that effects transparency of this bar.Note that opacity value are float values that ranges from 0 to 1, 0 being fully transparent and 1 being fully opaque.

The format to specify this element in the manifest.json file is : `"toolbar" : [R,G,B,opacity]`

Eg. `"toolbar" : [25, 154, 154, 0.5]`

Note that this element also specifies color value of the background for floating the status bar(in the bottom of page).It's found that using opacity values for this element makes the status bar transparent, but the text inside it will contain a opaque background of same color-hence area without the text will be transparent

  * #### tab\_text

This is a color element that specifies the color of the title text of the current tab(tab title name of current tab).

  * #### tab\_background\_text

This is a color element that specifies the color of the title text of all the inactive tabs/out of focus tabs.

  * #### bookmark\_text
This is a color element that specifies the color of the text of bookmarks in the toolbar and the text for the download bar that appears at the bottom.
Note : During a download, the text color indicating the number of MB downloaded is not configurable

  * #### ntp\_background

This is a color element that specifies the color of the background of the new tab page(covers all areas that is not mapped by [theme\_ntp\_background](#theme_ntp_background.md)).Usually if a alpha transparency is employed in the image element theme\_ntp\_background, make sure that ntp\_background is such that it matches that image element.

  * #### ntp\_text

This is a color element that specifies the color of all the text that appears in the new tab page.(tips, quick access lables,etc.,).

  * #### ntp\_link

This is a color element that specifes the color of all the links that may appear in the new tab page.(currently the links under list view and links of tips that appear at the bottom of new tab page takes it's color from this)

  * #### ntp\_link\_underline

This is a color element that specifies the color of the underline of all links in the new tab page(the color of underline of the ntp\_link element).

  * #### ntp\_header

This is a color element that specifies the color for the frame of quick link buttons, when one hovers the mouse over it.It also specifies the 1px border color of the [toolbar](#toolbar.md) element ,the ntp\_section element and the color of three small buttons in the new tab page-thumbnail view,list view,change page layout.

  * #### ntp\_section

This is a color element that specifies the color for the border of the quick link buttons(see help image) and also the background color for the recently closed bar that appears above the tips area.Similar to the [toolbar](#toolbar.md) element, this can als contain opacity value.

  * #### ntp\_section\_text

This is a clolor element that specifies the color of all the text that appears in the section area.(currently onl the text "Recently closed" derives it's color from this)

  * #### ntp\_section\_link

This is a color element that specifies the color of all the links that appear in the section area.Currently all the links in the "Recently closed" bar take their color from this.

  * #### ntp\_section\_link\_underline

This is a color element that specifies the color of underlines of all the links that appear in the section area.(underlines the ntp\_section\_link element)

  * #### control\_background

This should specify the color of the control buttons of window-minimize,maximize and close.But I couldn't confirm that.It seems that the following element overrides it.

  * #### button\_background

This is a color element that specifies the color for the background of all the buttons in the toolbar area(back,forward, bookmark,etc.,).This element too can contain opacity values like the [toolbar](#toolbar.md), which will affect the opacity of the window control buttons( minimize,maximize,close).

The following are tint elements.The tint element [buttons](#buttons.md) is the most common one, but you may include other elements too. Before moving on to those, one must know how the tins work.The tint elements are used to assign color tints to certain elements of the browser area.The value of the tint is in floating values ranging from 0 to 1. Eg, `"buttons" : [0.3,0.5,0.5]` (the values range from 0 to 1, hence even 0.125 or 0.65 represent a color).

  * Here the first value represents the hue value, for which 0 means red and 1 means red

  * The next is saturation value that lets you set vibrancy of the color,here 0 means completely desaturated and 1 means fully saturated.

  * The next value is lightness/brightness value.Here 0 means least bright and 1 means most bright
﻿
  * #### buttons

This is a tint element, that is used to specify a color tint for icons inside all the buttons in the toolbar (back, forward, refresh, etc.).

  * #### frame

This is a tint element, that is used to specify a color tint for the frame area.Whatever image you've created for the frame area will be tinted with a color that you specify here.

  * #### frame\_inactive

This is a tint element, similar to the tint element frame, but the tint is applied when the window is inactive/out of focus.

  * #### frame\_incognito

This is a tint element, that is used to specify a color tint for the frame area in incognito mode.Whatever image you've created for the frame area will be tinted with a color that you specify here.

  * #### frame\_incognito\_inactive

This is a tint element, that is used to specify a color tint for the frame area in incognito mode, but when the window is inactive/out of focus.

  * #### background\_tab

This is a tint element,that specifies the color tint of the inactive tabs in incognito mode.

  * #### ntp\_background\_alignment

This is a property element, that is used to control the alignment property of the image element [theme\_ntp\_background](#theme_ntp_background.md).The value for this element is entered as follows:
`"ntp_background_alignment" : "VALUE"`

In the place of VALUE, you can enter either "top","bottom","left" or "right".Further you can use combinations like "left top","right bottom",etc., The difference is that using only "left", aligns the background image to the left center of the new tab page.While using "left top" aligns the image to the top left corner of the new tab page.
Eg,  `"ntp_background_alignment" : "left bottom"`
_(Note that the default alignment of the background image is center)._

  * #### ntp\_background\_repeat

This is a property element, that is used to control the repetition of the image element [theme\_ntp\_background](#theme_ntp_background.md).It is specified as:

`"ntp_background_repeat" : "VALUE"`

In the place of VALUE, you can enter either "repeat","no-repeat","repeat-x" or "repeat-y" .Depending upon the image you've created as the background you can choose to repeat the image along x-axis or y-axis or turn repeat off, since repeat is on by default!.

  * #### ntp\_logo\_alternate

This is a propety element that specifies what header of Google chrome you wnat for your theme.It is specified as follows:

`"ntp_logo_alternate" : VALUE`

Note that this element's value should not be entered in double quotes!.In the place of VALUE you can enter 0 or 1.Choosing 0 will give you a colorful Google Chrome header logo inside the new tab page.Choosing 1 will give you an all white Google Chrome header logo inside the new tab page.

### Packaging

That ends the description of various theme elements.Once you've the images needed, and after creating the manifest.json file, you are ready to test your theme.In the latest beta version, you'ev the option to package the theme into an extension.To do this follow these steps(to know more about packaging visit [this link](http://code.google.com/chrome/extensions/packaging.html) ):
  1. Open the Chrome browser (it has to be the lates beta version).
  1. In the options menu (click the wrench in toolbar).
  1. Choose the Tools submenu, then Extensions.
  1. In the page that appears, click on the "Pack extension" button.
  1. You'll be asked to browse and locate the extension root directory-remember the folder    we created with the theme name?, the root directory is that one.
  1. In the dialog box that comes up, Click ok.

Now the theme has been packaged into an extension ( if there were errors in the manifest.json file, you'll be notified before the extension is created, and the extention will not be packaged until the error is rectified).Now open the extension file in chrome(it's located next to the root folder), you'll be asked if you want to continue-click continue and you'll se your theme.Once satisfied with the theme, you need to create a zip file of the root directory and submit to the [extensions gallery](https://chrome.google.com/extensions).