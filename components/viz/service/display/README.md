# display/

This directory is the implementation of the Display Compositor.

The Display Compositor combines frames submitted to the viz service through
frame sinks, and combines them into a single gpu or software resource to be
presented to the user.

This directory is agnostic w.r.t. platform-specific presentation details, which
are abstracted behind OutputSurface and SoftwareOutputDevice. Through these
APIs, it supports either compositing gpu resources (textures) into a single
texture/framebuffer, or software resources (bitmaps) into a single bitmap.
