# WebGPU CTS

The WebGPU CTS is written in TypeScript, and builds into two directories:

- `out/`: Built framework and test files, needed to run standalone or command line.
- `out-wpt/`: Build directory for export into WPT. Contains WPT runner and a copy of just the needed files from `out/`.

## Docs

- [Terminology used in the test framework](docs/terms.md)

## Developing

### Setup

After checking out the repository and installing Yarn, run these commands to
set up dependencies:

```sh
cd cts/
yarn install

yarn global add grunt-cli  # install grunt globally
grunt  # show available grunt commands
```

### Build

To build and run all pre-submit checks (including type and lint checks and
unittests), use:

```sh
grunt pre
```

To just build the project, use:

```sh
grunt build
```

### Run

To test in a browser under the standalone harness, run `grunt serve`, then
open:

* http://localhost:8080/ (defaults to ?runnow=0&q=cts)
* http://localhost:8080/?runnow=1&q=unittests
* http://localhost:8080/?runnow=1&q=unittests:basic&q=unittests:params

### Export to WPT

Copy the `out-wpt/` directory as the `webgpu/` directory in your WPT checkout.

### Making Changes

Since this project is written in TypeScript, it integrates best with Visual
Studio Code. There are also some default settings (in `.vscode/settings.json`)
which will be applied automatically.

Before uploading, you should run pre-submit checks (`grunt pre`).

Be sure to read [CONTRIBUTING.md](CONTRIBUTING.md).
