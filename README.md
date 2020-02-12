# WebGPU CTS

## >>> [**Contribution Guidelines**](https://github.com/gpuweb/gpuweb/wiki/WebGPU-CTS-guidelines) <<<

## Docs

- [Terminology used in the test framework](docs/terms.md)

## Developing

The WebGPU CTS is written in TypeScript, and builds into two directories:

- `out/`: Built framework and test files, needed to run standalone or command line.
- `out-wpt/`: Build directory for export into WPT. Contains WPT runner and a copy of just the needed files from `out/`.

### Setup

After checking out the repository and installing Yarn, run these commands to
set up dependencies:

```sh
cd cts/
yarn install

npx grunt  # show available grunt commands
```

### Build

To build and run all pre-submit checks (including type and lint checks and
unittests), use:

```sh
npx grunt pre
```

For a quicker iterative build:

```sh
npx grunt test
```

### Run

To test in a browser under the standalone harness, run `grunt serve`, then
open:

- http://localhost:8080/ (defaults to ?runnow=0&q=cts)
- http://localhost:8080/?runnow=1&q=unittests:
- http://localhost:8080/?runnow=1&q=unittests:basic:&q=unittests:params:

### Debug

To see debug logs in a browser, use the `debug=1` query string:

- http://localhost:8080/?q=cts:validation&debug=1

### Making Changes

To add new tests, simply imitate the pattern in neigboring tests or
neighboring files. New test files must be named ending in `.spec.ts`.

For an example, see `src/suites/cts/examples.spec.ts`.

Since this project is written in TypeScript, it integrates best with Visual
Studio Code. There are also some default settings (in `.vscode/settings.json`)
which will be applied automatically.

Before uploading, you should run pre-submit checks (`grunt pre`).

Be sure to read [CONTRIBUTING.md](CONTRIBUTING.md).

### Export to WPT

Copy (or symlink) the `out-wpt/` directory as the `webgpu/` directory in your
WPT checkout.
