module.exports = function(grunt) {
  const mkRun = (...args) => {
    return {
      cmd: "npx",
      args: [
        "ts-node",
        "--transpile-only",
        '-O{"module":"commonjs"}',
        "node-run.js",
        ...args
      ]
    };
  };

  // Project configuration.
  grunt.initConfig({
    pkg: grunt.file.readJSON("package.json"),

    clean: ["out/"],

    run: {
      "list-cts": mkRun("src/cts", "--generate-listing=out/cts/listing.json"),
      "run-cts": mkRun("src/cts", "--run"),
      "list-unittests": mkRun("src/unittests", "--generate-listing=out/unittests/listing.json"),
      "run-unittests": mkRun("src/unittests", "--run"),
    },

    "http-server": {
      ".": {
        root: ".",
        port: 8080,
        host: "127.0.0.1",
        cache: 5,
      }
    },

    ts: {
      "check": {
        tsconfig: {
          tsconfig: "tsconfig.json",
          passThrough: true,
        },
      },

      "out/": {
        tsconfig: {
          tsconfig: "tsconfig.json",
          passThrough: true,
        },
        options: {
          additionalFlags: "--noEmit false --outDir out/",
        }
      },
    },

    tslint: {
      options: {
        configuration: "tslint.json",
      },
      files: {
        src: [ "src/**/*.ts" ],
      },
    },
  });

  grunt.loadNpmTasks("grunt-contrib-clean");
  grunt.loadNpmTasks("grunt-http-server");
  grunt.loadNpmTasks("grunt-run");
  grunt.loadNpmTasks("grunt-ts");
  grunt.loadNpmTasks("grunt-tslint");

  const publishedTasks = [];
  function publishTask(name, desc, deps) {
    grunt.registerTask(name, deps);
    publishedTasks.push({name, desc});
  }

  publishTask("check", "Check Typescript build", [
    "ts:check",
  ]);
  publishTask("build", "Build out/", [
    "ts:out/",
    "run:list-cts",
    "run:list-unittests",
  ]);
  publishTask("serve", "Serve out/ on 127.0.0.1:8080", [
    "http-server:.",
  ]);
  publishedTasks.push({name: "clean", desc: "Clean out/"});

  publishTask("run-cts", "(Node) Run CTS", [ "run:run-cts" ]);
  publishTask("run-unittests", "(Node) Run unittests", [ "run:run-unittests" ]);

  grunt.registerTask("default", "", () => {
    console.log("Available tasks (see grunt --help for more):");
    for (const {name, desc} of publishedTasks) {
      console.log(`$ grunt ${name}`);
      console.log(`  ${desc}`);
    }
  });
};
