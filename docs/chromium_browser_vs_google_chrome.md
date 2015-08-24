Chromium on Linux has two general flavors: You can either get [Google Chrome](http://www.google.com/chrome?platform=linux) or chromium-browser (see LinuxChromiumPackages).  This page tries to describe the differences between the two.

In short, Google Chrome is the Chromium open source project built, packaged, and distributed by Google.  This table lists what Google adds to the Google Chrome builds **on Linux**.

| | **Google Chrome** | **Chromium** | **Extra notes** |
|:|:------------------|:-------------|:----------------|
| Logo | Colorful          | Blue         |
| [Crash reporting](LinuxCrashDumping.md) | Yes, if you turn it on | None         | Please include symbolized backtraces in bug reports if you don't have crash reporting |
| User metrics | Yes, if you turn it on | No           |
| Video and audio tags | AAC, H.264, MP3, Opus, Theora, Vorbis, VP8, VP9, and WAV  | Opus, Theora, Vorbis, VP8, VP9, and WAV by default | May vary by distro |
| Adobe Flash | Sandboxed PPAPI (non-free) plugin included in release | Supports NPAPI (unsandboxed) plugins, including the one from Adobe in Chrome 34 and below |
| Code | Tested by Chrome developers | May be modified by distributions | Please include distribution information if you report bugs |
| Sandbox  | Always on         | Depending on the distribution (navigate to about:sandbox to confirm) |                 |
| Package | Single deb/rpm    | Depending on the distribution |                 |
| Profile | Kept in `~/.config/google-chrome` | Kept in `~/.config/chromium` |
| Cache | Kept in `~/.cache/google-chrome` | Kept in `~/.cache/chromium` |
| Quality Assurance | New releases are tested before sending to users | Depending on the distribution | Distributions are encouraged to track stable channel releases: see http://googlechromereleases.blogspot.com/ , http://omahaproxy.appspot.com/ and http://gsdview.appspot.com/chromium-browser-official/ |
| Google API keys | Added by Google   | Depending on the distribution | See http://www.chromium.org/developers/how-tos/api-keys |