This directory contains code needed for rendering and interacting with
frame decorations in mash.

### Immersive fullscreen

Immersive fullscreen is a specialized fullscreen mode. In it the user
can move the mouse to the top of the screen and the window header
will slide down on top of the fullscreen window contents.

There are two distinct ways for immersive mode to work in mash:

1. Mash handles it all. This is the default. In this mode a separate
ui::Window is created for the reveal of the title area. HeaderView is used to
render the title area of the reveal in the separate window. The client
does nothing special here.

2. The client takes control of it all (as happens in chrome). To
enable this the client sets kDisableImmersive_InitProperty on the window. In 
this mode the client creates a separate window for the reveal (similar to
1). The reveal window is a child of the window going into immersive
mode. Mash knows to paint window decorations to the reveal window by way of
the property kRenderParentTitleArea_Property set on the parent
window. The client runs all the immersive logic, including positioning
of the reveal window.

In both cases DetachedTitleAreaRenderer and HeaderView is used to
render the title area. The difference is in the first case mash
creates the window, in the second case the child does (and tells mash
about it). And of course in the second case the client runs all the
immersive logic.
