module.exports = function(grunt) {
  const mkRun = (inspect, ...args) => {
    return {
      cmd: "node",
      args: [
        ...(inspect ? ["--inspect-brk"] : []),
        "-r", "ts-node/register/transpile-only",
        "./node-run.js",
        ...args
      ]
    };
  };

  // Project configuration.
  grunt.initConfig({
    pkg: grunt.file.readJSON("package.json"),

    clean: ["out/"],

    run: {
      "cts":       mkRun(false, "src/cts", "--run"),
      "debug-cts": mkRun(true,  "src/cts", "--run"),
      "list-cts":  mkRun(false, "src/cts", "--generate-listing=out/cts/listing.json"),
      "unittests":       mkRun(false, "src/unittests", "--run"),
      "debug-unittests": mkRun(true,  "src/unittests", "--run"),
      "list-unittests":  mkRun(false, "src/unittests", "--generate-listing=out/unittests/listing.json"),
      "demos":       mkRun(false, "src/demos", "--run"),
      "debug-demos": mkRun(true,  "src/demos", "--run"),
      "list-demos":  mkRun(false, "src/demos", "--generate-listing=out/demos/listing.json"),
      "build-out": {
        cmd: "npx",
        args: [
          "babel",
          "--source-maps", "true",
          "--extensions", ".ts",
          "--out-dir", "out/",
          "src/",
        ]
      },
      "build-shaderc": {
        cmd: "npx",
        args: [
          "babel",
          "--plugins", "babel-plugin-transform-commonjs-es2015-modules",
          "node_modules/@webgpu/shaderc/dist/index.js",
          "-o", "out/shaderc.js",
        ]
      }
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
          tsconfig: "tsconfig.web.json",
          passThrough: true,
        },
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
  publishedTasks.push({ name: "tslint", desc: "Run tslint" });
  publishTask("build", "Build out/", [
    "run:build-shaderc",
    "run:build-out",
    "run:list-cts",
    "run:list-unittests",
    "run:list-demos",
  ]);
  publishTask("serve", "Serve out/ on 127.0.0.1:8080", [
    "http-server:.",
  ]);
  publishedTasks.push({ name: "clean", desc: "Clean out/" });

  publishedTasks.push({ name: "run:{cts,unittests,demos}", desc: "(Node) Run {cts,unittests,demos}" });
  publishedTasks.push({ name: "run:debug-{cts,unittests,demos}", desc: "(Node) Debug {cts,unittests,demos}" });

  grunt.registerTask("default", "", () => {
    console.log("Available tasks (see grunt --help for info):");
    for (const {name, desc} of publishedTasks) {
      console.log(`$ grunt ${name}`);
      console.log(`  ${desc}`);
    }
  });
};
