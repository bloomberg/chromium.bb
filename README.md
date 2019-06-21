# cts-experiment

```sh
cd cts-experiment/
yarn install

yarn global add grunt-cli  # install grunt globally
grunt  # show available grunt commands

grunt build
tools/run.js cts
```

After `build` and `serve`, open:
* http://localhost:8080/?q=cts (default)
* http://localhost:8080/?q=demos
* http://localhost:8080/?q=unittests
* http://localhost:8080/?runnow=1&q=unittests:basic&q=unittests:params
