# Building Chromium on Debian

Generally speaking, follow the [Linux Build Instructions](linux_build_instructions.md).
However, you may need to update the package names in `install-build-deps.sh`:

*   libexpat-dev -> libexpat1-dev
*   freetype-dev -> libfreetype6-dev
*   libbzip2-dev -> libbz2-dev
*   libcupsys2-dev -> libcups2-dev

Additionally, if you're building Chromium components for Android, you'll need to
install the package: lib32z1
