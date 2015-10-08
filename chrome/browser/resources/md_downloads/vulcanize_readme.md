# Vulcanizing Material Design downloads

`vulcanize` is an npm module used to combine resources.  In order to make the
Material Design downloads page sufficiently fast, we run vulcanize on the source
files to combine them and reduce blocking load/parse time.

## Required software

Vulcanization currently requires:

- node.js: v0.10.25 (can be found with `node --version`)
- npm: 1.3.10 (can be found with `npm --version`)
- vulcanize: 1.12.3 (can be found with `vulcanize --version`)
- crisper: 1.0.7 (can be found with `npm info crisper`)

## Installing required software

For instructions on installing node and npm, see
[here](https://docs.npmjs.com/getting-started/installing-node).

Once you've installed npm, you can get `crisper` and `vulcanize` via:

```bash
$ sudo npm install -g crisper vulcanize
```

## Combining resources with vulcanize

To combine all the CSS/HTML/JS for the downloads page to make it production
fast, you can run the commands:

```bash
$ chrome/browser/resources/md_downloads/vulcanize.py  # from src/
```

This should overwrite the following files:

- chrome/browser/resources/md_downloads/
 - vulcanized.html (all <link rel=import> and stylesheets inlined)
 - crisper.js (all JavaScript, extracted from vulcanized.html)

## Testing downloads without vulcanizing

If you're locally working on the downloads page, you can simply load this URL to
bypass the vulcanized version: `chrome://downloads/dev.html`
