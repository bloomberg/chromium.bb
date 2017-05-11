# cc/

This directory contains a compositor. Its clients include blink and the browser
UI.

Design documents for the graphics stack can be found at
[chromium-graphics](https://www.chromium.org/developers/design-documents/chromium-graphics).

## Glossaries

### Stacked elements and stacking contexts

### Active CompositorFrame

### Active Tree
The set of layers and property trees that was/will be used to submit a
CompositorFrame from the layer compositor. Composited effects such as scrolling,
pinch, and animations are done by modifying the active tree, which allows for
producing and submitting a new CompositorFrame.

### CompositorFrame
A set of RenderPasses (which are a list of DrawQuads) along with metadata.
Conceptually this is the instructions (transforms, texture ids, etc) for how to
draw an entire scene which will be presented in a surface.

### CopyOutputRequest (or Copy Request)
A request for a texture (or bitmap) copy of some part of the compositor's
output. Such requests force the compositor to use a separate RenderPass for the
content to be copied, which allows it to do the copy operation once the
RenderPass has been drawn to.

### ElementID
Chosen by cc's clients and can be used as a stable identifier across updates.
For example, blink uses ElementIDs as a stable id for the object (opaque to cc)
that is responsible for a composited animation. Some additional information in
[element_id.h](https://codesearch.chromium.org/chromium/src/cc/trees/element_id.h)

### DirectRenderer
An abstraction that provides an API for the Display to draw a fully-aggregated
CompositorFrame to a physical output. Subclasses of it provide implementations
for various backends, currently GL or Software.

### Layer

### LayerImpl

### LayerTree

### Occlusion Culling
Avoiding work by skipping over things which are not visible due to being
occluded (hidden from sight by other opaque things in front of them). Most
commonly refers to skipping drawing (ie culling) of DrawQuads when other
DrawQuads will be in front and occluding them.

### Property Trees

### Display
A controller class that takes CompositorFrames for each surface and draws them
to a physical output.

### Draw
Filling pixels in a physical output (technically could be to an offscreen
texture), but this is the final output of the display compositor.

### DrawQuad
A unit of work for drawing. Each DrawQuad has its own texture id, transform,
offset, etc.

### Shared Quad State
A shared set of states used by multiple draw quads. DrawQuads that are linked to
the same shared quad state will all use the same properties from it, with the
addition of things found on their individual DrawQuad structures.

### Render Pass
A list of DrawQuads which will all be drawn together into the same render target
(either a texture or physical output). Most times all DrawQuads are part of a
single RenderPass. Additional RenderPasses are used for effects that require a
set of DrawQuads to be drawn together into a buffer first, with the effect
applied then to the buffer instead of each individual DrawQuad.

### Render Surface
Synonym for RenderPass now. Historically part of the Layer tree data structures,
with a 1:1 mapping to RenderPasses. RenderSurfaceImpl is a legacy piece that
remains.

### Surface

### Record

### Raster

### Paint

### Pending CompositorFrame

### Pending Tree
The set of layers and property trees that is generated from a main frame (or
BeginMainFrame, or commit). The pending tree exists to do raster work in the
layer compositor without clobbering the active tree until it is done. This
allows the active tree to be used in the meantime.

### Composite
To produce a single graphical output from multiple inputs. In practice, the
layer compositor does raster from recordings and manages memory, performs
composited effects such as scrolling, pinch, animations, producing a
CompositorFrame. The display compositor does an actual "composite" to draw the
final output into a single physical output.

### Invalidation

### Damage

### Tiles

### Prepare Tiles
Prioritize and schedule needed tiles for raster. This is the entry point to a
system that converts painting (raster sources / recording sources) into
rasterized resources that live on tiles. This also kicks off any dependent image
decodes for images that need to be decode for the raster to take place.

### Device Scale Factor
The scale at which we want to display content on the output device. For very
high resolution monitors, everything would become too small if just presented
1:1 with the pixels. So we use a larger number of physical pixels per logical
pixels. This ratio is the device scale factor. 1 or 2 is the most common on
ChromeOS. Values between 1 and 2 are common on Windows.
