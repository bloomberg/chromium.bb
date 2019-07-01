# cts-experiment

### Build

```sh
cd cts-experiment/
yarn install

yarn global add grunt-cli  # install grunt globally
grunt  # show available grunt commands

grunt build
```

### Run

From the command-line (unittests only, for now):

```sh
tools/run unittests
```

In a browser:

```sh
grunt serve
```

Then open:

* http://localhost:8080/ (defaults to ?runnow=0&q=cts)
* http://localhost:8080/?runnow=1&q=unittests
* http://localhost:8080/?runnow=1&q=unittests:basic&q=unittests:params

### Making Changes

To run pre-submit checks, run:

```
grunt pre
```

Be sure to read [CONTRIBUTING.md](CONTRIBUTING.md).
