# Vectorized icons in native Chrome UI

## Background

Chrome can draw vectorized images using Skia. Vector images have the advantages of looking better at different scale factors or sizes, can be easily colorized at runtime, and reduce the chrome binary size.

Chrome uses .icon files to describe vector icons. This is a bespoke file format which is actually a C++ array definition. At build time, the .icon files are composed into a .cc file which is compiled into the binary.

Vector icons can be found in various vector_icons subdirectories throughout the code base. Use `components/vector_icons/` for generic icons shared among many directories and components, or more specific directories such as  `ui/views/vector_icons` or `ash/resources/vector_icons` for less widely used icons.

Some of the icons have **.1x.icon** variants which are used when the device scale factor is 100%. For any other scale factor, the **.icon** variant will be used. The 1x variants are generally only necessary for very small icons which may look fuzzy if shrunk from a larger icon.

## Converting an SVG to .icon format

[This tool](http://evanstade.github.io/skiafy/) generates .icon file output from SVGs. (If you want to contribute improvements, [here's the project](https://github.com/evanstade/skiafy).)

It handles only a small subset of SVG (paths, circles, etc.) and it's finicky about what it expects as the format, but with a minor amount of manual intervention beforehand, it mostly spits out usable .icon output. It will often work better if you run the SVG through SVGO first, which is a separate project (an SVG minifier). [Jake Archibald's SVGOMG](https://jakearchibald.github.io/svgomg/) is a web interface to SVGO. If any manual adjustments need to be made to the output, the [SVG Path spec](https://www.w3.org/TR/SVG/paths.html) is a helpful reference.

Some SVGs are already pretty minimal, like the ones at [the Material Design Icon repository](https://material.io/icons/) so they don't require much if any adjustment, but some SVG editing tools like Sketch leave a lot of random cruft so SVGOMG helps a lot. Take the output and insert into a .icon file.

## Using .icon files

Once you have created an .icon file, place it in an appropriate vector_icon subdirectory and add the filename to the corresponding BUILD.gn. A constant is automatically generated so that the icon can be referenced at runtime. The icon file foo_bar.icon is mapped to the constant name of kFooBarIcon ('k' + camel-cased filename + 'Icon') and a sample call site to create this icon looks something like:

    gfx::CreateVectorIcon(kFooBarIcon, 32, color_utils::DeriveDefaultIconColor(text_color));

If the size argument is unspecified, the size will be taken from the .icon file (or the .1x.icon if more than one exists). The icon's name should match its identifier on [the MD icons site](https://material.io/icons/) if that's where it came from. For example, `ic_accessibility` would become `accessibility.icon`.

## FAQ

### Where can I use vector icons?

Chrome's native UI on desktop platforms. Currently the vector icons are in extensive use on Views platforms, where Skia is the normal drawing tool. Mac uses them sometimes, but optimizing performance is still a [TODO](http://crbug.com/595035) so many places stick with raster assets. The files in `chrome/app/theme/default_*_percent/legacy` are ones that have been switched to vector icons for Views but not yet for OS X. If you need to add raster assets (PNG) for mobile or OS X, please make sure to limit their inclusion to those platforms.

### How can I preview vector icons?

Use [this extension](https://github.com/sadrulhc/vector-icons) to preview icons in [codesearch](http://cs.chromium.org/).

You can also build and run the `views_examples_exe` (or `views_examples_with_content_exe`) target and select "Vector Icons" from the dropdown menu. This loads a simple interface which allows you view a provided vector icon file at a specified size and color. Contributions to improve this interface are welcome ([bug](https://bugs.chromium.org/p/chromium/issues/detail?id=630295)).

### Can my vector icon have more than one color?

Yes. You can hard-code colors for specific path elements by adding a `PATH_COLOR_ARGB` command to the appropriate place within the .icon file. Any path elements which are not given a hard-coded color in this manner will use the color provided to `CreateVectorIcon()` at runtime.


### When introducing a new icon, should I use a PNG or a vector icon?

Use a vector icon, unless the icon is extremely complex (e.g., a product logo). Also see above, "Where can I use vector icons?"
